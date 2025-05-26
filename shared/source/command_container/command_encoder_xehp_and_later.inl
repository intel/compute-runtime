/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/scratch_space_controller.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_walk_order.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/helpers/pause_on_gpu_properties.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/ray_tracing_helper.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"
#include "shared/source/kernel/implicit_args_helper.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helper/release_helper.h"

#include "encode_dispatch_kernel_args_ext.h"
#include "encode_surface_state_args.h"

#include <algorithm>
#include <type_traits>

namespace NEO {

template <typename Family>
template <typename WalkerType>
void EncodeDispatchKernel<Family>::encode(CommandContainer &container, EncodeDispatchKernelArgs &args) {
    using STATE_BASE_ADDRESS = typename Family::STATE_BASE_ADDRESS;

    UNRECOVERABLE_IF(args.makeCommandView && (args.cpuWalkerBuffer == nullptr || args.cpuPayloadBuffer == nullptr));

    constexpr bool heaplessModeEnabled = Family::template isHeaplessMode<WalkerType>();
    const HardwareInfo &hwInfo = args.device->getHardwareInfo();
    auto &rootDeviceEnvironment = args.device->getRootDeviceEnvironment();

    const auto &kernelDescriptor = args.dispatchInterface->getKernelDescriptor();
    auto sizeCrossThreadData = args.dispatchInterface->getCrossThreadDataSize();
    auto sizePerThreadData = args.dispatchInterface->getPerThreadDataSize();
    auto sizePerThreadDataForWholeGroup = args.dispatchInterface->getPerThreadDataSizeForWholeThreadGroup();
    auto pImplicitArgs = args.dispatchInterface->getImplicitArgs();

    LinearStream *listCmdBufferStream = container.getCommandStream();

    auto threadGroupDims = static_cast<const uint32_t *>(args.threadGroupDimensions);
    uint32_t threadDimsVec[3] = {0, 0, 0};
    if (!args.isIndirect) {
        threadDimsVec[0] = threadGroupDims[0];
        threadDimsVec[1] = threadGroupDims[1];
        threadDimsVec[2] = threadGroupDims[2];
    }

    if (!args.makeCommandView) {
        bool systolicModeRequired = kernelDescriptor.kernelAttributes.flags.usesSystolicPipelineSelectMode;
        if (container.systolicModeSupportRef() && (container.lastPipelineSelectModeRequiredRef() != systolicModeRequired)) {
            container.lastPipelineSelectModeRequiredRef() = systolicModeRequired;
            EncodeComputeMode<Family>::adjustPipelineSelect(container, kernelDescriptor);
        }
    }

    WalkerType walkerCmd = Family::template getInitGpuWalker<WalkerType>();
    auto &idd = walkerCmd.getInterfaceDescriptor();

    EncodeDispatchKernel<Family>::setGrfInfo(&idd, kernelDescriptor.kernelAttributes.numGrfRequired, sizeCrossThreadData,
                                             sizePerThreadData, rootDeviceEnvironment);

    bool localIdsGenerationByRuntime = args.dispatchInterface->requiresGenerationOfLocalIdsByRuntime();
    auto requiredWorkgroupOrder = args.dispatchInterface->getRequiredWorkgroupOrder();

    {
        auto isaAllocation = args.dispatchInterface->getIsaAllocation();
        UNRECOVERABLE_IF(nullptr == isaAllocation);

        uint64_t kernelStartPointer = args.dispatchInterface->getIsaOffsetInParentAllocation();
        if constexpr (heaplessModeEnabled) {
            kernelStartPointer += isaAllocation->getGpuAddress();
        } else {
            kernelStartPointer += isaAllocation->getGpuAddressToPatch();
        }

        if (!localIdsGenerationByRuntime) {
            kernelStartPointer += kernelDescriptor.entryPoints.skipPerThreadDataLoad;
        }
        idd.setKernelStartPointer(kernelStartPointer);
    }
    if (args.dispatchInterface->getKernelDescriptor().kernelAttributes.flags.usesAssert && args.device->getL0Debugger() != nullptr) {
        idd.setSoftwareExceptionEnable(1);
    }

    auto threadsPerThreadGroup = args.dispatchInterface->getNumThreadsPerThreadGroup();
    idd.setNumberOfThreadsInGpgpuThreadGroup(threadsPerThreadGroup);

    EncodeDispatchKernel<Family>::programBarrierEnable(idd,
                                                       kernelDescriptor,
                                                       hwInfo);

    EncodeDispatchKernel<Family>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, args.defaultPipelinedThreadArbitrationPolicy);

    auto releaseHelper = rootDeviceEnvironment.getReleaseHelper();
    auto slmSize = EncodeDispatchKernel<Family>::computeSlmValues(hwInfo, args.dispatchInterface->getSlmTotalSize(), releaseHelper, heaplessModeEnabled);

    if (debugManager.flags.OverrideSlmAllocationSize.get() != -1) {
        slmSize = static_cast<uint32_t>(debugManager.flags.OverrideSlmAllocationSize.get());
    }
    idd.setSharedLocalMemorySize(slmSize);

    auto bindingTableStateCount = kernelDescriptor.payloadMappings.bindingTable.numEntries;
    bool sshProgrammingRequired = true;

    auto &productHelper = args.device->getProductHelper();
    if (productHelper.isSkippingStatefulInformationRequired(kernelDescriptor)) {
        bindingTableStateCount = 0u;
        sshProgrammingRequired = false;
    }

    if (sshProgrammingRequired && !args.makeCommandView) {
        bool isBindlessKernel = NEO::KernelDescriptor::isBindlessAddressingKernel(kernelDescriptor);
        if (isBindlessKernel) {
            bool globalBindlessSsh = args.device->getBindlessHeapsHelper() != nullptr;
            auto sshHeapSize = args.dispatchInterface->getSurfaceStateHeapDataSize();

            if (sshHeapSize > 0u) {
                auto ssh = args.surfaceStateHeap;
                if (ssh == nullptr) {
                    container.prepareBindfulSsh();
                    ssh = container.getHeapWithRequiredSizeAndAlignment(HeapType::surfaceState, sshHeapSize, NEO::EncodeDispatchKernel<Family>::getDefaultSshAlignment());
                }

                uint64_t bindlessSshBaseOffset = ptrDiff(ssh->getSpace(0), ssh->getCpuBase());
                if (globalBindlessSsh) {
                    bindlessSshBaseOffset += ptrDiff(ssh->getGraphicsAllocation()->getGpuAddress(), ssh->getGraphicsAllocation()->getGpuBaseAddress());
                }

                DEBUG_BREAK_IF(bindingTableStateCount > 0u);

                if (bindingTableStateCount == 0) {
                    // Allocate space for new ssh data
                    auto dstSurfaceState = ssh->getSpace(sshHeapSize);
                    memcpy_s(dstSurfaceState, sshHeapSize, args.dispatchInterface->getSurfaceStateHeapData(), sshHeapSize);
                }
                args.dispatchInterface->patchBindlessOffsetsInCrossThreadData(bindlessSshBaseOffset);
            }

        } else {
            if constexpr (heaplessModeEnabled == false) {
                if (bindingTableStateCount > 0u) {
                    auto ssh = args.surfaceStateHeap;
                    if (ssh == nullptr) {
                        container.prepareBindfulSsh();
                        ssh = container.getHeapWithRequiredSizeAndAlignment(HeapType::surfaceState, args.dispatchInterface->getSurfaceStateHeapDataSize(), NEO::EncodeDispatchKernel<Family>::getDefaultSshAlignment());
                    }
                    auto bindingTablePointer = static_cast<uint32_t>(EncodeSurfaceState<Family>::pushBindingTableAndSurfaceStates(
                        *ssh,
                        args.dispatchInterface->getSurfaceStateHeapData(),
                        args.dispatchInterface->getSurfaceStateHeapDataSize(), bindingTableStateCount,
                        kernelDescriptor.payloadMappings.bindingTable.tableOffset));

                    idd.setBindingTablePointer(bindingTablePointer);
                }
            }
        }
    }

    auto preemptionMode = args.device->getDebugger() ? PreemptionMode::ThreadGroup : args.preemptionMode;
    PreemptionHelper::programInterfaceDescriptorDataPreemption<Family>(&idd, preemptionMode);

    uint32_t samplerCount = 0;

    if constexpr (Family::supportsSampler) {
        if (args.device->getDeviceInfo().imageSupport && !args.makeCommandView) {

            if (kernelDescriptor.payloadMappings.samplerTable.numSamplers > 0) {
                auto dsHeap = args.dynamicStateHeap;
                if (dsHeap == nullptr) {
                    dsHeap = container.getIndirectHeap(HeapType::dynamicState);
                    auto dshSizeRequired = NEO::EncodeDispatchKernel<Family>::getSizeRequiredDsh(kernelDescriptor, container.getNumIddPerBlock());
                    if (dsHeap->getAvailableSpace() <= dshSizeRequired) {
                        dsHeap = container.getHeapWithRequiredSizeAndAlignment(HeapType::dynamicState, dsHeap->getMaxAvailableSpace(), NEO::EncodeDispatchKernel<Family>::getDefaultDshAlignment());
                    }
                }
                UNRECOVERABLE_IF(!dsHeap);

                auto bindlessHeapsHelper = args.device->getBindlessHeapsHelper();
                samplerCount = kernelDescriptor.payloadMappings.samplerTable.numSamplers;
                uint64_t samplerStateOffset = EncodeStates<Family>::copySamplerState(
                    dsHeap, kernelDescriptor.payloadMappings.samplerTable.tableOffset,
                    kernelDescriptor.payloadMappings.samplerTable.numSamplers,
                    kernelDescriptor.payloadMappings.samplerTable.borderColor,
                    args.dispatchInterface->getDynamicStateHeapData(),
                    bindlessHeapsHelper, rootDeviceEnvironment);

                if (bindlessHeapsHelper && !bindlessHeapsHelper->isGlobalDshSupported()) {
                    // add offset of graphics allocation base address relative to heap base address
                    samplerStateOffset += static_cast<uint32_t>(ptrDiff(dsHeap->getGpuBase(), bindlessHeapsHelper->getGlobalHeapsBase()));
                }
                if (heaplessModeEnabled && bindlessHeapsHelper) {
                    samplerStateOffset += bindlessHeapsHelper->getGlobalHeapsBase();
                }

                args.dispatchInterface->patchSamplerBindlessOffsetsInCrossThreadData(samplerStateOffset);
                if constexpr (!heaplessModeEnabled) {
                    idd.setSamplerStatePointer(static_cast<uint32_t>(samplerStateOffset));
                }
            }
        }
    }

    if constexpr (heaplessModeEnabled == false) {
        EncodeDispatchKernel<Family>::adjustBindingTablePrefetch(idd, samplerCount, bindingTableStateCount);
    }

    uint64_t offsetThreadData = 0u;
    constexpr uint32_t inlineDataSize = WalkerType::getInlineDataSize();
    auto crossThreadData = args.dispatchInterface->getCrossThreadData();

    uint32_t inlineDataProgrammingOffset = 0u;
    bool inlineDataProgramming = EncodeDispatchKernel<Family>::inlineDataProgrammingRequired(kernelDescriptor);
    if (inlineDataProgramming) {
        inlineDataProgrammingOffset = std::min(inlineDataSize, sizeCrossThreadData);
        auto dest = reinterpret_cast<char *>(walkerCmd.getInlineDataPointer());
        memcpy_s(dest, inlineDataSize, crossThreadData, inlineDataProgrammingOffset);
        sizeCrossThreadData -= inlineDataProgrammingOffset;
        crossThreadData = ptrOffset(crossThreadData, inlineDataProgrammingOffset);
        inlineDataProgramming = inlineDataProgrammingOffset != 0;
    }

    auto scratchAddressForImmediatePatching = EncodeDispatchKernel<Family>::getScratchAddressForImmediatePatching<heaplessModeEnabled>(container, args);
    uint32_t sizeThreadData = sizePerThreadDataForWholeGroup + sizeCrossThreadData;
    uint32_t sizeForImplicitArgsPatching = NEO::ImplicitArgsHelper::getSizeForImplicitArgsPatching(pImplicitArgs, kernelDescriptor, !localIdsGenerationByRuntime, rootDeviceEnvironment);
    uint32_t sizeForImplicitArgsStruct = NEO::ImplicitArgsHelper::getSizeForImplicitArgsStruct(pImplicitArgs, kernelDescriptor, true, rootDeviceEnvironment);
    uint32_t iohRequiredSize = sizeThreadData + sizeForImplicitArgsPatching + args.reserveExtraPayloadSpace;
    IndirectParamsInInlineDataArgs encodeIndirectParamsArgs{};
    {
        void *ptr = nullptr;
        if (!args.makeCommandView) {
            auto heap = container.getIndirectHeap(HeapType::indirectObject);
            UNRECOVERABLE_IF(!heap);
            heap->align(Family::cacheLineSize);

            if (args.isKernelDispatchedFromImmediateCmdList) {
                ptr = container.getHeapWithRequiredSizeAndAlignment(HeapType::indirectObject, iohRequiredSize, Family::indirectDataAlignment)->getSpace(iohRequiredSize);
            } else {
                ptr = container.getHeapSpaceAllowGrow(HeapType::indirectObject, iohRequiredSize);
            }

            offsetThreadData = (is64bit ? heap->getHeapGpuStartOffset() : heap->getHeapGpuBase()) + static_cast<uint64_t>(heap->getUsed() - sizeThreadData - args.reserveExtraPayloadSpace);
            if (pImplicitArgs) {
                offsetThreadData -= sizeForImplicitArgsStruct;
                pImplicitArgs->setLocalIdTablePtr(heap->getGraphicsAllocation()->getGpuAddress() + heap->getUsed() - iohRequiredSize);
                EncodeDispatchKernel<Family>::patchScratchAddressInImplicitArgs<heaplessModeEnabled>(*pImplicitArgs, scratchAddressForImmediatePatching, args.immediateScratchAddressPatching);

                ptr = NEO::ImplicitArgsHelper::patchImplicitArgs(ptr, *pImplicitArgs, kernelDescriptor, std::make_pair(!localIdsGenerationByRuntime, requiredWorkgroupOrder), rootDeviceEnvironment, &args.outImplicitArgsPtr);
            }

            if (args.isIndirect) {
                auto gpuPtr = heap->getGraphicsAllocation()->getGpuAddress() + static_cast<uint64_t>(heap->getUsed() - sizeThreadData - inlineDataProgrammingOffset);
                uint64_t implicitArgsGpuPtr = 0u;
                if (pImplicitArgs) {
                    implicitArgsGpuPtr = gpuPtr + inlineDataProgrammingOffset - sizeForImplicitArgsStruct;
                }
                EncodeIndirectParams<Family>::encode(container, gpuPtr, args.dispatchInterface, implicitArgsGpuPtr, &encodeIndirectParamsArgs);
            }
        } else {
            ptr = args.cpuPayloadBuffer;
        }

        if (sizeCrossThreadData > 0) {
            memcpy_s(ptr, sizeCrossThreadData,
                     crossThreadData, sizeCrossThreadData);
        }

        auto perThreadDataPtr = args.dispatchInterface->getPerThreadData();
        if (perThreadDataPtr != nullptr) {
            ptr = ptrOffset(ptr, sizeCrossThreadData);
            memcpy_s(ptr, sizePerThreadDataForWholeGroup,
                     perThreadDataPtr, sizePerThreadDataForWholeGroup);
        }
    }

    if (args.isHeaplessStateInitEnabled == false && !args.makeCommandView) {
        if (container.isAnyHeapDirty() ||
            args.requiresUncachedMocs) {

            PipeControlArgs syncArgs;
            syncArgs.dcFlushEnable = args.postSyncArgs.dcFlushEnable;
            MemorySynchronizationCommands<Family>::addSingleBarrier(*container.getCommandStream(), syncArgs);
            STATE_BASE_ADDRESS sbaCmd;
            auto gmmHelper = container.getDevice()->getGmmHelper();
            uint32_t statelessMocsIndex =
                args.requiresUncachedMocs ? (gmmHelper->getUncachedMOCS() >> 1) : (gmmHelper->getL3EnabledMOCS() >> 1);
            auto l1CachePolicy = container.l1CachePolicyDataRef()->getL1CacheValue(false);
            auto l1CachePolicyDebuggerActive = container.l1CachePolicyDataRef()->getL1CacheValue(true);

            EncodeStateBaseAddressArgs<Family> encodeStateBaseAddressArgs = {
                &container,                  // container
                sbaCmd,                      // sbaCmd
                nullptr,                     // sbaProperties
                statelessMocsIndex,          // statelessMocsIndex
                l1CachePolicy,               // l1CachePolicy
                l1CachePolicyDebuggerActive, // l1CachePolicyDebuggerActive
                args.partitionCount > 1,     // multiOsContextCapable
                args.isRcs,                  // isRcs
                container.doubleSbaWaRef(),  // doubleSbaWa
                heaplessModeEnabled          // heaplessModeEnabled
            };
            EncodeStateBaseAddress<Family>::encode(encodeStateBaseAddressArgs);
            container.setDirtyStateForAllHeaps(false);

            bool sbaTrackingEnabled = NEO::Debugger::isDebugEnabled(args.isInternal) && args.device->getL0Debugger();
            NEO::EncodeStateBaseAddress<Family>::setSbaTrackingForL0DebuggerIfEnabled(sbaTrackingEnabled,
                                                                                      *args.device,
                                                                                      *container.getCommandStream(),
                                                                                      sbaCmd, container.isUsingPrimaryBuffer());
        }
    }

    if (!args.makeCommandView) {
        if (NEO::PauseOnGpuProperties::pauseModeAllowed(NEO::debugManager.flags.PauseOnEnqueue.get(), args.device->debugExecutionCounter.load(), NEO::PauseOnGpuProperties::PauseMode::BeforeWorkload)) {
            void *commandBuffer = listCmdBufferStream->getSpace(MemorySynchronizationCommands<Family>::getSizeForBarrierWithPostSyncOperation(rootDeviceEnvironment, NEO::PostSyncMode::noWrite));
            args.additionalCommands->push_back(commandBuffer);

            EncodeSemaphore<Family>::applyMiSemaphoreWaitCommand(*listCmdBufferStream, *args.additionalCommands);
        }
    }

    uint8_t *inlineDataPtr = reinterpret_cast<uint8_t *>(walkerCmd.getInlineDataPointer());
    EncodeDispatchKernel<Family>::programInlineDataHeapless<heaplessModeEnabled>(inlineDataPtr, args, container, offsetThreadData, scratchAddressForImmediatePatching);

    if constexpr (heaplessModeEnabled == false) {
        if (!args.makeCommandView) {
            walkerCmd.setIndirectDataStartAddress(static_cast<uint32_t>(offsetThreadData));
            walkerCmd.setIndirectDataLength(sizeThreadData);
        }
    }
    container.getIndirectHeap(HeapType::indirectObject)->align(NEO::EncodeDispatchKernel<Family>::getDefaultIOHAlignment());

    EncodeDispatchKernel<Family>::encodeThreadData(walkerCmd,
                                                   nullptr,
                                                   threadGroupDims,
                                                   args.dispatchInterface->getGroupSize(),
                                                   kernelDescriptor.kernelAttributes.simdSize,
                                                   kernelDescriptor.kernelAttributes.numLocalIdChannels,
                                                   threadsPerThreadGroup,
                                                   args.dispatchInterface->getThreadExecutionMask(),
                                                   localIdsGenerationByRuntime,
                                                   inlineDataProgramming,
                                                   args.isIndirect,
                                                   requiredWorkgroupOrder,
                                                   rootDeviceEnvironment);

    if (args.postSyncArgs.inOrderExecInfo) {
        EncodePostSync<Family>::setupPostSyncForInOrderExec(walkerCmd, args.postSyncArgs);
    } else if (args.postSyncArgs.isRegularEvent()) {
        EncodePostSync<Family>::setupPostSyncForRegularEvent(walkerCmd, args.postSyncArgs);
    } else {
        EncodeDispatchKernel<Family>::forceComputeWalkerPostSyncFlushWithWrite(walkerCmd);
    }

    if (debugManager.flags.ForceComputeWalkerPostSyncFlush.get() == 1) {
        auto &postSync = walkerCmd.getPostSync();
        postSync.setDataportPipelineFlush(true);
        postSync.setDataportSubsliceCacheFlush(true);
    }

    walkerCmd.setPredicateEnable(args.isPredicate);

    auto threadGroupCount = walkerCmd.getThreadGroupIdXDimension() * walkerCmd.getThreadGroupIdYDimension() * walkerCmd.getThreadGroupIdZDimension();
    EncodeDispatchKernel<Family>::encodeThreadGroupDispatch(idd, *args.device, hwInfo, threadDimsVec, threadGroupCount,
                                                            kernelDescriptor.kernelMetadata.requiredThreadGroupDispatchSize, kernelDescriptor.kernelAttributes.numGrfRequired, threadsPerThreadGroup, walkerCmd);
    if (debugManager.flags.PrintKernelDispatchParameters.get()) {
        fprintf(stdout, "kernel, %s, grfCount, %d, simdSize, %d, tilesCount, %d, implicitScaling, %s, threadGroupCount, %d, numberOfThreadsInGpgpuThreadGroup, %d, threadGroupDimensions, %d, %d, %d, threadGroupDispatchSize enum, %d\n",
                kernelDescriptor.kernelMetadata.kernelName.c_str(),
                kernelDescriptor.kernelAttributes.numGrfRequired,
                kernelDescriptor.kernelAttributes.simdSize,
                args.device->getNumSubDevices(),
                ImplicitScalingHelper::isImplicitScalingEnabled(args.device->getDeviceBitfield(), true) ? "Yes" : "no",
                threadGroupCount,
                idd.getNumberOfThreadsInGpgpuThreadGroup(),
                walkerCmd.getThreadGroupIdXDimension(),
                walkerCmd.getThreadGroupIdYDimension(),
                walkerCmd.getThreadGroupIdZDimension(),
                idd.getThreadGroupDispatchSize());
    }

    EncodeDispatchKernel<Family>::setupPreferredSlmSize(&idd, rootDeviceEnvironment, threadsPerThreadGroup,
                                                        args.dispatchInterface->getSlmTotalSize(),
                                                        args.dispatchInterface->getSlmPolicy());

    auto kernelExecutionType = args.isCooperative ? KernelExecutionType::concurrent : KernelExecutionType::defaultType;

    EncodeWalkerArgs walkerArgs{
        .argsExtended = args.extendedArgs,
        .kernelExecutionType = kernelExecutionType,
        .requiredDispatchWalkOrder = args.requiredDispatchWalkOrder,
        .localRegionSize = args.localRegionSize,
        .maxFrontEndThreads = args.device->getDeviceInfo().maxFrontEndThreads,
        .requiredSystemFence = args.postSyncArgs.requiresSystemMemoryFence(),
        .hasSample = kernelDescriptor.kernelAttributes.flags.hasSample,
        .l0DebuggerEnabled = args.device->getL0Debugger() != nullptr};

    EncodeDispatchKernel<Family>::encodeAdditionalWalkerFields(rootDeviceEnvironment, walkerCmd, walkerArgs);
    EncodeDispatchKernel<Family>::encodeWalkerPostSyncFields(walkerCmd, rootDeviceEnvironment, walkerArgs);
    EncodeDispatchKernel<Family>::encodeComputeDispatchAllWalker(walkerCmd, &idd, rootDeviceEnvironment, walkerArgs);

    EncodeDispatchKernel<Family>::overrideDefaultValues(walkerCmd, idd);

    uint32_t workgroupSize = args.dispatchInterface->getGroupSize()[0] * args.dispatchInterface->getGroupSize()[1] * args.dispatchInterface->getGroupSize()[2];
    bool isRequiredDispatchWorkGroupOrder = args.requiredDispatchWalkOrder != NEO::RequiredDispatchWalkOrder::none;
    if (args.partitionCount > 1 && !args.isInternal) {
        const uint64_t workPartitionAllocationGpuVa = args.device->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocationGpuAddress();

        ImplicitScalingDispatchCommandArgs implicitScalingArgs{
            workPartitionAllocationGpuVa,                                                            // workPartitionAllocationGpuVa
            args.device,                                                                             // device
            &args.outWalkerPtr,                                                                      // outWalkerPtr
            args.requiredPartitionDim,                                                               // requiredPartitionDim
            args.partitionCount,                                                                     // partitionCount
            workgroupSize,                                                                           // workgroupSize
            threadGroupCount,                                                                        // threadGroupCount
            args.maxWgCountPerTile,                                                                  // maxWgCountPerTile
            !(container.getFlushTaskUsedForImmediate() || container.isUsingPrimaryBuffer()),         // useSecondaryBatchBuffer
            !args.isKernelDispatchedFromImmediateCmdList,                                            // apiSelfCleanup
            args.postSyncArgs.dcFlushEnable,                                                         // dcFlush
            EncodeDispatchKernel<Family>::singleTileExecImplicitScalingRequired(args.isCooperative), // forceExecutionOnSingleTile
            args.makeCommandView,                                                                    // blockDispatchToCommandBuffer
            isRequiredDispatchWorkGroupOrder};                                                       // isRequiredDispatchWorkGroupOrder

        ImplicitScalingDispatch<Family>::dispatchCommands(*listCmdBufferStream,
                                                          walkerCmd,
                                                          args.device->getDeviceBitfield(),
                                                          implicitScalingArgs);
        args.partitionCount = implicitScalingArgs.partitionCount;
    } else {
        args.partitionCount = 1;
        EncodeDispatchKernel<Family>::setWalkerRegionSettings(walkerCmd, *args.device, args.partitionCount, workgroupSize, threadGroupCount, args.maxWgCountPerTile, isRequiredDispatchWorkGroupOrder);

        if (!args.makeCommandView) {
            auto buffer = listCmdBufferStream->getSpaceForCmd<WalkerType>();
            args.outWalkerPtr = buffer;
            *buffer = walkerCmd;
        }
    }

    if (args.isIndirect) {
        auto walkerGpuVa = listCmdBufferStream->getGpuBase() + ptrDiff(args.outWalkerPtr, listCmdBufferStream->getCpuBase());
        EncodeIndirectParams<Family>::applyInlineDataGpuVA(encodeIndirectParamsArgs, walkerGpuVa + ptrDiff(walkerCmd.getInlineDataPointer(), &walkerCmd));
    }

    if (args.cpuWalkerBuffer) {
        *reinterpret_cast<WalkerType *>(args.cpuWalkerBuffer) = walkerCmd;
    }

    if (!args.makeCommandView) {

        if (NEO::PauseOnGpuProperties::pauseModeAllowed(NEO::debugManager.flags.PauseOnEnqueue.get(), args.device->debugExecutionCounter.load(), NEO::PauseOnGpuProperties::PauseMode::AfterWorkload)) {
            void *commandBuffer = listCmdBufferStream->getSpace(MemorySynchronizationCommands<Family>::getSizeForBarrierWithPostSyncOperation(rootDeviceEnvironment, NEO::PostSyncMode::noWrite));
            args.additionalCommands->push_back(commandBuffer);

            EncodeSemaphore<Family>::applyMiSemaphoreWaitCommand(*listCmdBufferStream, *args.additionalCommands);
        }
    }
}

template <typename Family>
template <typename CommandType>
void EncodePostSync<Family>::setupPostSyncForRegularEvent(CommandType &cmd, const EncodePostSyncArgs &args) {
    using POSTSYNC_DATA = decltype(Family::template getPostSyncType<CommandType>());

    auto &postSync = getPostSync(cmd, 0);

    auto operationType = POSTSYNC_DATA::OPERATION_WRITE_IMMEDIATE_DATA;
    uint64_t gpuVa = args.eventAddress;
    uint64_t immData = args.postSyncImmValue;

    if (args.isTimestampEvent) {
        operationType = POSTSYNC_DATA::OPERATION_WRITE_TIMESTAMP;
        immData = 0;

        UNRECOVERABLE_IF(!(isAligned<timestampDestinationAddressAlignment>(gpuVa)));
    } else {
        UNRECOVERABLE_IF(!(isAligned<immWriteDestinationAddressAlignment>(gpuVa)));
    }
    uint32_t mocs = getPostSyncMocs(args.device->getRootDeviceEnvironment(), args.dcFlushEnable);
    setPostSyncData(postSync, operationType, gpuVa, immData, 0, mocs, false, false);

    encodeL3Flush(cmd, args);
    adjustTimestampPacket(cmd, args);
}

template <typename Family>
template <typename PostSyncT>
void EncodePostSync<Family>::setPostSyncDataCommon(PostSyncT &postSyncData, typename PostSyncT::OPERATION operation, uint64_t gpuVa, uint64_t immData) {
    postSyncData.setOperation(operation);
    postSyncData.setImmediateData(immData);
    postSyncData.setDestinationAddress(gpuVa);
}

template <typename Family>
inline uint32_t EncodePostSync<Family>::getPostSyncMocs(const RootDeviceEnvironment &rootDeviceEnvironment, bool dcFlush) {
    auto gmmHelper = rootDeviceEnvironment.getGmmHelper();

    if (debugManager.flags.OverridePostSyncMocs.get() != -1) {
        return debugManager.flags.OverridePostSyncMocs.get();
    }

    if (dcFlush) {
        return gmmHelper->getUncachedMOCS();
    } else {
        return gmmHelper->getL3EnabledMOCS();
    }
}

template <typename Family>
bool EncodeDispatchKernel<Family>::isRuntimeLocalIdsGenerationRequired(uint32_t activeChannels,
                                                                       const size_t *lws,
                                                                       std::array<uint8_t, 3> walkOrder,
                                                                       bool requireInputWalkOrder,
                                                                       uint32_t &requiredWalkOrder,
                                                                       uint32_t simd) {
    if (simd == 1) {
        return true;
    }
    bool hwGenerationOfLocalIdsEnabled = true;
    if (debugManager.flags.EnableHwGenerationLocalIds.get() != -1) {
        hwGenerationOfLocalIdsEnabled = !!debugManager.flags.EnableHwGenerationLocalIds.get();
    }
    if (hwGenerationOfLocalIdsEnabled) {
        if (activeChannels == 0) {
            return false;
        }

        size_t totalLwsSize = 1u;
        for (auto dimension = 0u; dimension < activeChannels; dimension++) {
            totalLwsSize *= lws[dimension];
        }

        if (totalLwsSize > 1024u) {
            return true;
        }

        // check if we need to follow kernel requirements
        if (requireInputWalkOrder) {
            for (uint32_t dimension = 0; dimension < activeChannels - 1; dimension++) {
                if (!Math::isPow2<size_t>(lws[walkOrder[dimension]])) {
                    return true;
                }
            }

            auto index = 0u;
            while (index < HwWalkOrderHelper::walkOrderPossibilties) {
                if (walkOrder[0] == HwWalkOrderHelper::compatibleDimensionOrders[index][0] &&
                    walkOrder[1] == HwWalkOrderHelper::compatibleDimensionOrders[index][1]) {
                    break;
                }
                index++;
            }
            DEBUG_BREAK_IF(index >= HwWalkOrderHelper::walkOrderPossibilties);

            requiredWalkOrder = index;
            return false;
        }

        // kernel doesn't specify any walk order requirements, check if we have any compatible
        for (uint32_t walkOrder = 0; walkOrder < HwWalkOrderHelper::walkOrderPossibilties; walkOrder++) {
            bool allDimensionsCompatible = true;
            for (uint32_t dimension = 0; dimension < activeChannels - 1; dimension++) {
                if (!Math::isPow2<size_t>(lws[HwWalkOrderHelper::compatibleDimensionOrders[walkOrder][dimension]])) {
                    allDimensionsCompatible = false;
                    break;
                }
            }
            if (allDimensionsCompatible) {
                requiredWalkOrder = walkOrder;
                return false;
            }
        }
    }
    return true;
}

template <typename Family>
template <typename WalkerType>
void EncodeDispatchKernel<Family>::encodeThreadData(WalkerType &walkerCmd,
                                                    const uint32_t *startWorkGroup,
                                                    const uint32_t *numWorkGroups,
                                                    const uint32_t *workGroupSizes,
                                                    uint32_t simd,
                                                    uint32_t localIdDimensions,
                                                    uint32_t threadsPerThreadGroup,
                                                    uint32_t threadExecutionMask,
                                                    bool localIdsGenerationByRuntime,
                                                    bool inlineDataProgrammingRequired,
                                                    bool isIndirect,
                                                    uint32_t requiredWorkGroupOrder,
                                                    const RootDeviceEnvironment &rootDeviceEnvironment) {

    if (isIndirect) {
        walkerCmd.setIndirectParameterEnable(true);
    } else {
        walkerCmd.setThreadGroupIdXDimension(static_cast<uint32_t>(numWorkGroups[0]));
        walkerCmd.setThreadGroupIdYDimension(static_cast<uint32_t>(numWorkGroups[1]));
        walkerCmd.setThreadGroupIdZDimension(static_cast<uint32_t>(numWorkGroups[2]));
    }

    if (startWorkGroup) {
        walkerCmd.setThreadGroupIdStartingX(static_cast<uint32_t>(startWorkGroup[0]));
        walkerCmd.setThreadGroupIdStartingY(static_cast<uint32_t>(startWorkGroup[1]));
        walkerCmd.setThreadGroupIdStartingZ(static_cast<uint32_t>(startWorkGroup[2]));
    }

    uint64_t executionMask = threadExecutionMask;
    if (executionMask == 0) {
        auto workGroupSize = workGroupSizes[0] * workGroupSizes[1] * workGroupSizes[2];
        auto remainderSimdLanes = workGroupSize & (simd - 1);
        executionMask = maxNBitValue(remainderSimdLanes);
        if (!executionMask) {
            executionMask = maxNBitValue(isSimd1(simd) ? 32 : simd);
        }
    }

    walkerCmd.setExecutionMask(static_cast<uint32_t>(executionMask));
    walkerCmd.setSimdSize(getSimdConfig<WalkerType>(simd));

    walkerCmd.setMessageSimd(walkerCmd.getSimdSize());

    if (debugManager.flags.ForceSimdMessageSizeInWalker.get() != -1) {
        walkerCmd.setMessageSimd(debugManager.flags.ForceSimdMessageSizeInWalker.get());
    }

    // 1) cross-thread inline data will be put into R1, but if kernel uses local ids, then cross-thread should be put further back
    // so whenever local ids are driver or hw generated, reserve space by setting right values for emitLocalIds
    // 2) Auto-generation of local ids should be possible, when in fact local ids are used
    if (!localIdsGenerationByRuntime && localIdDimensions > 0) {
        UNRECOVERABLE_IF(localIdDimensions > 3);
        uint32_t emitLocalIdsForDim = (1 << 0);

        if (localIdDimensions > 1) {
            emitLocalIdsForDim |= (1 << 1);
        }
        if (localIdDimensions > 2) {
            emitLocalIdsForDim |= (1 << 2);
        }
        walkerCmd.setEmitLocalId(emitLocalIdsForDim);

        walkerCmd.setLocalXMaximum(static_cast<uint32_t>(workGroupSizes[0] - 1));
        walkerCmd.setLocalYMaximum(static_cast<uint32_t>(workGroupSizes[1] - 1));
        walkerCmd.setLocalZMaximum(static_cast<uint32_t>(workGroupSizes[2] - 1));

        walkerCmd.setGenerateLocalId(1);
        walkerCmd.setWalkOrder(requiredWorkGroupOrder);
    }

    adjustWalkOrder(walkerCmd, requiredWorkGroupOrder, rootDeviceEnvironment);
    if (inlineDataProgrammingRequired == true) {
        walkerCmd.setEmitInlineParameter(1);
    }
}

template <typename Family>
inline bool EncodeDispatchKernel<Family>::isDshNeeded(const DeviceInfo &deviceInfo) {
    if constexpr (Family::supportsSampler) {
        return deviceInfo.imageSupport;
    }
    return false;
}

template <typename Family>
void EncodeStateBaseAddress<Family>::setSbaAddressesForDebugger(NEO::Debugger::SbaAddresses &sbaAddress, const STATE_BASE_ADDRESS &sbaCmd) {
    sbaAddress.bindlessSurfaceStateBaseAddress = sbaCmd.getBindlessSurfaceStateBaseAddress();
    sbaAddress.dynamicStateBaseAddress = sbaCmd.getDynamicStateBaseAddress();
    sbaAddress.generalStateBaseAddress = sbaCmd.getGeneralStateBaseAddress();
    sbaAddress.instructionBaseAddress = sbaCmd.getInstructionBaseAddress();
    sbaAddress.surfaceStateBaseAddress = sbaCmd.getSurfaceStateBaseAddress();
    sbaAddress.indirectObjectBaseAddress = 0;
}

template <typename Family>
void EncodeStateBaseAddress<Family>::encode(EncodeStateBaseAddressArgs<Family> &args) {
    auto &device = *args.container->getDevice();
    auto gmmHelper = device.getRootDeviceEnvironment().getGmmHelper();

    auto dsh = args.container->isHeapDirty(HeapType::dynamicState) ? args.container->getIndirectHeap(HeapType::dynamicState) : nullptr;
    auto ioh = args.container->isHeapDirty(HeapType::indirectObject) ? args.container->getIndirectHeap(HeapType::indirectObject) : nullptr;
    auto ssh = args.container->isHeapDirty(HeapType::surfaceState) ? args.container->getIndirectHeap(HeapType::surfaceState) : nullptr;
    auto isDebuggerActive = device.getDebugger() != nullptr;
    bool setGeneralStateBaseAddress = args.sbaProperties ? false : true;
    uint64_t globalHeapsBase = 0;
    uint64_t bindlessSurfStateBase = 0;
    bool useGlobalSshAndDsh = false;

    if (device.getBindlessHeapsHelper()) {
        bindlessSurfStateBase = device.getBindlessHeapsHelper()->getGlobalHeapsBase();
        globalHeapsBase = device.getBindlessHeapsHelper()->getGlobalHeapsBase();
        useGlobalSshAndDsh = true;
    }

    StateBaseAddressHelperArgs<Family> stateBaseAddressHelperArgs = {
        0,                                                  // generalStateBaseAddress
        args.container->getIndirectObjectHeapBaseAddress(), // indirectObjectHeapBaseAddress
        args.container->getInstructionHeapBaseAddress(),    // instructionHeapBaseAddress
        globalHeapsBase,                                    // globalHeapsBaseAddress
        0,                                                  // surfaceStateBaseAddress
        bindlessSurfStateBase,                              // bindlessSurfaceStateBaseAddress
        &args.sbaCmd,                                       // stateBaseAddressCmd
        args.sbaProperties,                                 // sbaProperties
        dsh,                                                // dsh
        ioh,                                                // ioh
        ssh,                                                // ssh
        gmmHelper,                                          // gmmHelper
        args.statelessMocsIndex,                            // statelessMocsIndex
        args.l1CachePolicy,                                 // l1CachePolicy
        args.l1CachePolicyDebuggerActive,                   // l1CachePolicyDebuggerActive
        NEO::MemoryCompressionState::notApplicable,         // memoryCompressionState
        true,                                               // setInstructionStateBaseAddress
        setGeneralStateBaseAddress,                         // setGeneralStateBaseAddress
        useGlobalSshAndDsh,                                 // useGlobalHeapsBaseAddress
        args.multiOsContextCapable,                         // isMultiOsContextCapable
        false,                                              // areMultipleSubDevicesInContext
        false,                                              // overrideSurfaceStateBaseAddress
        isDebuggerActive,                                   // isDebuggerActive
        args.doubleSbaWa,                                   // doubleSbaWa
        args.heaplessModeEnabled                            // heaplessModeEnabled
    };

    StateBaseAddressHelper<Family>::programStateBaseAddressIntoCommandStream(stateBaseAddressHelperArgs,
                                                                             *args.container->getCommandStream());

    if (args.sbaProperties) {
        if (args.sbaProperties->bindingTablePoolBaseAddress.value != StreamProperty64::initValue) {
            StateBaseAddressHelper<Family>::programBindingTableBaseAddress(*args.container->getCommandStream(),
                                                                           static_cast<uint64_t>(args.sbaProperties->bindingTablePoolBaseAddress.value),
                                                                           static_cast<uint32_t>(args.sbaProperties->bindingTablePoolSize.value),
                                                                           gmmHelper);
        }
    } else if (args.container->isHeapDirty(HeapType::surfaceState) && ssh != nullptr) {
        auto heap = args.container->getIndirectHeap(HeapType::surfaceState);
        StateBaseAddressHelper<Family>::programBindingTableBaseAddress(*args.container->getCommandStream(),
                                                                       *heap,
                                                                       gmmHelper);
    }
}

template <typename Family>
size_t EncodeStateBaseAddress<Family>::getRequiredSizeForStateBaseAddress(Device &device, CommandContainer &container, bool isRcs) {
    if constexpr (!Family::isHeaplessRequired()) {
        auto &hwInfo = device.getHardwareInfo();
        auto &productHelper = device.getProductHelper();

        size_t size = sizeof(typename Family::STATE_BASE_ADDRESS);
        if (productHelper.isAdditionalStateBaseAddressWARequired(hwInfo)) {
            size += sizeof(typename Family::STATE_BASE_ADDRESS);
        }
        if (container.isHeapDirty(HeapType::surfaceState)) {
            size += sizeof(typename Family::_3DSTATE_BINDING_TABLE_POOL_ALLOC);
        }

        return size;
    } else {
        UNRECOVERABLE_IF(true);
        return 0;
    }
}

template <typename Family>
inline void EncodeMediaInterfaceDescriptorLoad<Family>::encode(CommandContainer &container, IndirectHeap *childDsh) {}

template <typename Family>
void EncodeSurfaceState<Family>::encodeExtraBufferParams(EncodeSurfaceStateArgs &args) {
    auto surfaceState = reinterpret_cast<R_SURFACE_STATE *>(args.outMemory);
    Gmm *gmm = args.allocation ? args.allocation->getDefaultGmm() : nullptr;
    uint32_t compressionFormat = 0;

    bool setConstCachePolicy = false;
    if (args.allocation && args.allocation->getAllocationType() == AllocationType::constantSurface) {
        setConstCachePolicy = true;
    }

    if (surfaceState->getMemoryObjectControlState() == args.gmmHelper->getL3EnabledMOCS() &&
        debugManager.flags.ForceL1Caching.get() != 0) {
        setConstCachePolicy = true;
    }

    if (setConstCachePolicy == true) {
        surfaceState->setMemoryObjectControlState(args.gmmHelper->getL1EnabledMOCS());
    }

    encodeExtraCacheSettings(surfaceState, args);

    if (EncodeSurfaceState<Family>::isAuxModeEnabled(surfaceState, gmm)) {
        auto resourceFormat = gmm->gmmResourceInfo->getResourceFormat();
        compressionFormat = args.gmmHelper->getClientContext()->getSurfaceStateCompressionFormat(resourceFormat);

        if (debugManager.flags.ForceBufferCompressionFormat.get() != -1) {
            compressionFormat = debugManager.flags.ForceBufferCompressionFormat.get();
        }
    }

    if (debugManager.flags.EnableStatelessCompressionWithUnifiedMemory.get()) {
        if (args.allocation && !MemoryPoolHelper::isSystemMemoryPool(args.allocation->getMemoryPool())) {
            setCoherencyType(surfaceState, R_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT);
            setBufferAuxParamsForCCS(surfaceState);
            compressionFormat = debugManager.flags.FormatForStatelessCompressionWithUnifiedMemory.get();
        }
    }

    surfaceState->setCompressionFormat(compressionFormat);
}

template <typename Family>
void EncodeSemaphore<Family>::programMiSemaphoreWait(MI_SEMAPHORE_WAIT *cmd,
                                                     uint64_t compareAddress,
                                                     uint64_t compareData,
                                                     COMPARE_OPERATION compareMode,
                                                     bool registerPollMode,
                                                     bool waitMode,
                                                     bool useQwordData,
                                                     bool indirect,
                                                     bool switchOnUnsuccessful) {
    MI_SEMAPHORE_WAIT localCmd = Family::cmdInitMiSemaphoreWait;
    localCmd.setCompareOperation(compareMode);
    localCmd.setSemaphoreDataDword(static_cast<uint32_t>(compareData));
    localCmd.setSemaphoreGraphicsAddress(compareAddress);
    localCmd.setWaitMode(waitMode ? MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE : MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_SIGNAL_MODE);
    localCmd.setRegisterPollMode(registerPollMode ? MI_SEMAPHORE_WAIT::REGISTER_POLL_MODE::REGISTER_POLL_MODE_REGISTER_POLL : MI_SEMAPHORE_WAIT::REGISTER_POLL_MODE::REGISTER_POLL_MODE_MEMORY_POLL);
    localCmd.setIndirectSemaphoreDataDword(indirect);

    EncodeSemaphore<Family>::appendSemaphoreCommand(localCmd, compareData, indirect, useQwordData, switchOnUnsuccessful);

    *cmd = localCmd;
}

template <typename Family>
inline void EncodeWA<Family>::encodeAdditionalPipelineSelect(LinearStream &stream, const PipelineSelectArgs &args, bool is3DPipeline,
                                                             const RootDeviceEnvironment &rootDeviceEnvironment, bool isRcs) {}

template <typename Family>
inline size_t EncodeWA<Family>::getAdditionalPipelineSelectSize(Device &device, bool isRcs) {
    return 0u;
}
template <typename Family>
inline void EncodeWA<Family>::addPipeControlPriorToNonPipelinedStateCommand(LinearStream &commandStream, PipeControlArgs args,
                                                                            const RootDeviceEnvironment &rootDeviceEnvironment, bool isRcs) {

    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto *releaseHelper = rootDeviceEnvironment.getReleaseHelper();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs, releaseHelper);

    if (isExtendedWARequired) {
        args.textureCacheInvalidationEnable = true;
        args.hdcPipelineFlush = true;
        args.amfsFlushEnable = true;
        args.instructionCacheInvalidateEnable = true;
        args.constantCacheInvalidationEnable = true;
        args.stateCacheInvalidationEnable = true;

        args.dcFlushEnable = false;

        NEO::EncodeWA<Family>::setAdditionalPipeControlFlagsForNonPipelineStateCommand(args);
    } else if (isBasicWARequired) {
        args.hdcPipelineFlush = true;

        NEO::EncodeWA<Family>::setAdditionalPipeControlFlagsForNonPipelineStateCommand(args);
    }

    MemorySynchronizationCommands<Family>::addSingleBarrier(commandStream, args);
}

template <typename Family>
inline void EncodeStoreMemory<Family>::programStoreDataImm(MI_STORE_DATA_IMM *cmdBuffer,
                                                           uint64_t gpuAddress,
                                                           uint32_t dataDword0,
                                                           uint32_t dataDword1,
                                                           bool storeQword,
                                                           bool workloadPartitionOffset) {
    MI_STORE_DATA_IMM storeDataImmediate = Family::cmdInitStoreDataImm;
    storeDataImmediate.setAddress(gpuAddress);
    storeDataImmediate.setStoreQword(storeQword);
    storeDataImmediate.setDataDword0(dataDword0);
    if (storeQword) {
        storeDataImmediate.setDataDword1(dataDword1);
        storeDataImmediate.setDwordLength(MI_STORE_DATA_IMM::DWORD_LENGTH::DWORD_LENGTH_STORE_QWORD);
    } else {
        storeDataImmediate.setDwordLength(MI_STORE_DATA_IMM::DWORD_LENGTH::DWORD_LENGTH_STORE_DWORD);
    }
    storeDataImmediate.setWorkloadPartitionIdOffsetEnable(workloadPartitionOffset);
    EncodeStoreMemory<Family>::encodeForceCompletionCheck(storeDataImmediate);
    *cmdBuffer = storeDataImmediate;
}

template <typename Family>
inline void EncodeStoreMMIO<Family>::appendFlags(MI_STORE_REGISTER_MEM *storeRegMem, bool workloadPartition) {
    storeRegMem->setMmioRemapEnable(true);
    storeRegMem->setWorkloadPartitionIdOffsetEnable(workloadPartition);
}

template <typename Family>
size_t EncodeDispatchKernel<Family>::additionalSizeRequiredDsh(uint32_t iddCount) {
    return 0u;
}

template <typename Family>
inline size_t EncodeDispatchKernel<Family>::getInlineDataOffset(EncodeDispatchKernelArgs &args) {
    using DefaultWalkerType = typename Family::DefaultWalkerType;
    return offsetof(DefaultWalkerType, TheStructure.Common.InlineData);
}

template <typename Family>
template <typename WalkerType>
void EncodeDispatchKernel<Family>::forceComputeWalkerPostSyncFlushWithWrite(WalkerType &walkerCmd) {
    using POSTSYNC_DATA = decltype(Family::template getPostSyncType<WalkerType>());
    using OperationType = typename POSTSYNC_DATA::OPERATION;

    if (debugManager.flags.ForceComputeWalkerPostSyncFlushWithWrite.get() != -1) {
        auto &postSync = walkerCmd.getPostSync();
        postSync.setDataportPipelineFlush(true);
        postSync.setDataportSubsliceCacheFlush(true);
        postSync.setDestinationAddress(static_cast<uint64_t>(debugManager.flags.ForceComputeWalkerPostSyncFlushWithWrite.get()));
        postSync.setOperation(OperationType::OPERATION_WRITE_IMMEDIATE_DATA);
        postSync.setImmediateData(0u);
    }
}

template <typename Family>
uint32_t EncodeDispatchKernel<Family>::alignSlmSize(uint32_t slmSize) {
    const uint32_t alignedSlmSizes[] = {
        0u,
        1u * MemoryConstants::kiloByte,
        2u * MemoryConstants::kiloByte,
        4u * MemoryConstants::kiloByte,
        8u * MemoryConstants::kiloByte,
        16u * MemoryConstants::kiloByte,
        24u * MemoryConstants::kiloByte,
        32u * MemoryConstants::kiloByte,
        48u * MemoryConstants::kiloByte,
        64u * MemoryConstants::kiloByte,
        96u * MemoryConstants::kiloByte,
        128u * MemoryConstants::kiloByte,
    };

    for (auto &alignedSlmSize : alignedSlmSizes) {
        if (slmSize <= alignedSlmSize) {
            return alignedSlmSize;
        }
    }

    UNRECOVERABLE_IF(true);
    return 0;
}

template <typename Family>
uint32_t EncodeDispatchKernel<Family>::computeSlmValues(const HardwareInfo &hwInfo, uint32_t slmSize, ReleaseHelper *releaseHelper, bool isHeapless) {
    using SHARED_LOCAL_MEMORY_SIZE = typename Family::INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE;

    if (slmSize == 0u) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_0K;
    }

    UNRECOVERABLE_IF(slmSize > 128u * MemoryConstants::kiloByte);

    if (slmSize > 96u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_128K;
    }
    if (slmSize > 64u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_96K;
    }
    if (slmSize > 48u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_64K;
    }
    if (slmSize > 32u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_48K;
    }
    if (slmSize > 24u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_32K;
    }
    if (slmSize > 16u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_24K;
    }
    if (slmSize > 8u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_16K;
    }
    if (slmSize > 4u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_8K;
    }
    if (slmSize > 2u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_4K;
    }
    if (slmSize > 1u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_2K;
    }
    return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_1K;
}

template <typename Family>
template <typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::setupPreferredSlmSize(InterfaceDescriptorType *pInterfaceDescriptor, const RootDeviceEnvironment &rootDeviceEnvironment, const uint32_t threadsPerThreadGroup, uint32_t slmTotalSize, SlmPolicy slmPolicy) {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename InterfaceDescriptorType::PREFERRED_SLM_ALLOCATION_SIZE;
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    const uint32_t threadsPerDssCount = EncodeDispatchKernel<Family>::getThreadCountPerSubslice(hwInfo);
    const uint32_t workGroupCountPerDss = static_cast<uint32_t>(Math::divideAndRoundUp(threadsPerDssCount, threadsPerThreadGroup));

    slmTotalSize = EncodeDispatchKernel<Family>::alignPreferredSlmSize(slmTotalSize);

    uint32_t slmSize = 0u;

    switch (slmPolicy) {
    case SlmPolicy::slmPolicyLargeData:
        slmSize = slmTotalSize;
        break;
    case SlmPolicy::slmPolicyLargeSlm:
    default:
        slmSize = slmTotalSize * workGroupCountPerDss;
        break;
    }

    constexpr bool isHeapless = Family::template isInterfaceDescriptorHeaplessMode<InterfaceDescriptorType>();

    auto releaseHelper = rootDeviceEnvironment.getReleaseHelper();
    const auto &sizeToPreferredSlmValueArray = releaseHelper->getSizeToPreferredSlmValue(isHeapless);

    uint32_t programmableIdPreferredSlmSize = 0;
    for (auto &range : sizeToPreferredSlmValueArray) {
        if (slmSize <= range.upperLimit) {
            programmableIdPreferredSlmSize = range.valueToProgram;
            break;
        }
    }

    if (debugManager.flags.OverridePreferredSlmAllocationSizePerDss.get() != -1) {
        programmableIdPreferredSlmSize = static_cast<uint32_t>(debugManager.flags.OverridePreferredSlmAllocationSizePerDss.get());
    }

    pInterfaceDescriptor->setPreferredSlmAllocationSize(static_cast<PREFERRED_SLM_ALLOCATION_SIZE>(programmableIdPreferredSlmSize));
}

template <typename Family>
size_t EncodeStates<Family>::getSshHeapSize() {
    return 2 * MemoryConstants::megaByte;
}

template <typename Family>
template <typename WalkerType, typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::overrideDefaultValues(WalkerType &walkerCmd, InterfaceDescriptorType &interfaceDescriptor) {
    int32_t forceL3PrefetchForComputeWalker = debugManager.flags.ForceL3PrefetchForComputeWalker.get();
    if (forceL3PrefetchForComputeWalker != -1) {
        walkerCmd.setL3PrefetchDisable(!forceL3PrefetchForComputeWalker);
    }
}

template <typename Family>
template <typename WalkerType, typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::encodeThreadGroupDispatch(InterfaceDescriptorType &interfaceDescriptor, const Device &device, const HardwareInfo &hwInfo,
                                                             const uint32_t *threadGroupDimensions, const uint32_t threadGroupCount, const uint32_t requiredThreadGroupDispatchSize,
                                                             const uint32_t grfCount, const uint32_t threadsPerThreadGroup, WalkerType &walkerCmd) {
    const auto &productHelper = device.getProductHelper();

    if (requiredThreadGroupDispatchSize != 0) {
        interfaceDescriptor.setThreadGroupDispatchSize(static_cast<typename InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE>(requiredThreadGroupDispatchSize));
    } else if (productHelper.isDisableOverdispatchAvailable(hwInfo)) {
        interfaceDescriptor.setThreadGroupDispatchSize(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1);

        bool adjustTGDispatchSize = true;
        if (debugManager.flags.AdjustThreadGroupDispatchSize.get() != -1) {
            adjustTGDispatchSize = !!debugManager.flags.AdjustThreadGroupDispatchSize.get();
        }
        // apply v2 algorithm only for parts where MaxSubSlicesSupported is equal to SubSliceCount
        auto algorithmVersion = hwInfo.gtSystemInfo.MaxSubSlicesSupported == hwInfo.gtSystemInfo.SubSliceCount ? 2 : 1;
        if (debugManager.flags.ForceThreadGroupDispatchSizeAlgorithm.get() != -1) {
            algorithmVersion = debugManager.flags.ForceThreadGroupDispatchSizeAlgorithm.get();
        }

        auto tileCount = ImplicitScalingHelper::isImplicitScalingEnabled(device.getDeviceBitfield(), true) ? device.getNumSubDevices() : 1u;

        if (algorithmVersion == 2) {
            auto threadsPerXeCore = hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.MaxSubSlicesSupported;
            if (grfCount == 256) {
                threadsPerXeCore /= 2;
            }
            auto tgDispatchSizeSelected = 8;

            if (threadGroupDimensions[0] > 1 && (threadGroupDimensions[1] > 1 || threadGroupDimensions[2] > 1)) {
                while (threadGroupDimensions[0] % tgDispatchSizeSelected != 0) {
                    tgDispatchSizeSelected /= 2;
                }
            } else if (threadGroupDimensions[1] > 1 && threadGroupDimensions[2] > 1) {
                while (threadGroupDimensions[1] % tgDispatchSizeSelected != 0) {
                    tgDispatchSizeSelected /= 2;
                }
            }

            // make sure we fit all xe core
            while (threadGroupCount / tgDispatchSizeSelected < hwInfo.gtSystemInfo.MaxSubSlicesSupported * tileCount && tgDispatchSizeSelected > 1) {
                tgDispatchSizeSelected /= 2;
            }

            auto threadCountPerGrouping = tgDispatchSizeSelected * threadsPerThreadGroup;
            // make sure we do not use more threads then present on each xe core
            while (threadCountPerGrouping > threadsPerXeCore && tgDispatchSizeSelected > 1) {
                tgDispatchSizeSelected /= 2;
                threadCountPerGrouping /= 2;
            }

            if (tgDispatchSizeSelected == 8) {
                interfaceDescriptor.setThreadGroupDispatchSize(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_8);
            } else if (tgDispatchSizeSelected == 1) {
                interfaceDescriptor.setThreadGroupDispatchSize(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1);
            } else if (tgDispatchSizeSelected == 2) {
                interfaceDescriptor.setThreadGroupDispatchSize(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_2);
            } else {
                interfaceDescriptor.setThreadGroupDispatchSize(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_4);
            }
        } else {
            if (adjustTGDispatchSize) {
                UNRECOVERABLE_IF(grfCount == 0u);
                constexpr uint32_t maxThreadsInTGForTGDispatchSize8 = 16u;
                constexpr uint32_t maxThreadsInTGForTGDispatchSize4 = 32u;
                auto &gfxCoreHelper = device.getGfxCoreHelper();
                uint32_t availableThreadCount = gfxCoreHelper.calculateAvailableThreadCount(hwInfo, grfCount);
                availableThreadCount *= tileCount;

                uint32_t dispatchedTotalThreadCount = threadsPerThreadGroup * threadGroupCount;
                UNRECOVERABLE_IF(threadsPerThreadGroup == 0u);
                auto tgDispatchSizeSelected = 1u;

                if (dispatchedTotalThreadCount <= availableThreadCount) {
                    tgDispatchSizeSelected = 1;
                } else if (threadsPerThreadGroup <= maxThreadsInTGForTGDispatchSize8) {
                    tgDispatchSizeSelected = 8;
                } else if (threadsPerThreadGroup <= maxThreadsInTGForTGDispatchSize4) {
                    tgDispatchSizeSelected = 4;
                } else {
                    tgDispatchSizeSelected = 2;
                }
                if (threadGroupDimensions[0] > 1 && (threadGroupDimensions[1] > 1 || threadGroupDimensions[2] > 1)) {
                    while (threadGroupDimensions[0] % tgDispatchSizeSelected != 0) {
                        tgDispatchSizeSelected /= 2;
                    }
                } else if (threadGroupDimensions[1] > 1 && threadGroupDimensions[2] > 1) {
                    while (threadGroupDimensions[1] % tgDispatchSizeSelected != 0) {
                        tgDispatchSizeSelected /= 2;
                    }
                }
                if (tgDispatchSizeSelected == 8) {
                    interfaceDescriptor.setThreadGroupDispatchSize(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_8);
                } else if (tgDispatchSizeSelected == 1) {
                    interfaceDescriptor.setThreadGroupDispatchSize(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1);
                } else if (tgDispatchSizeSelected == 2) {
                    interfaceDescriptor.setThreadGroupDispatchSize(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_2);
                } else {
                    interfaceDescriptor.setThreadGroupDispatchSize(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_4);
                }
            }
        }
    }

    if (debugManager.flags.ForceThreadGroupDispatchSize.get() != -1) {
        interfaceDescriptor.setThreadGroupDispatchSize(static_cast<typename InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE>(
            debugManager.flags.ForceThreadGroupDispatchSize.get()));
    }
}

template <typename Family>
template <typename WalkerType>
void EncodeDispatchKernel<Family>::encodeWalkerPostSyncFields(WalkerType &walkerCmd, const RootDeviceEnvironment &rootDeviceEnvironment, const EncodeWalkerArgs &walkerArgs) {
    auto programGlobalFenceAsPostSyncOperationInComputeWalker = rootDeviceEnvironment.getProductHelper().isGlobalFenceInPostSyncRequired(*rootDeviceEnvironment.getHardwareInfo()) && walkerArgs.requiredSystemFence;
    int32_t overrideProgramSystemMemoryFence = debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.get();
    if (overrideProgramSystemMemoryFence != -1) {
        programGlobalFenceAsPostSyncOperationInComputeWalker = !!overrideProgramSystemMemoryFence;
    }
    auto &postSyncData = walkerCmd.getPostSync();
    postSyncData.setSystemMemoryFenceRequest(programGlobalFenceAsPostSyncOperationInComputeWalker);
}

template <typename Family>
void EncodeSurfaceState<Family>::encodeExtraCacheSettings(R_SURFACE_STATE *surfaceState, const EncodeSurfaceStateArgs &args) {
    using L1_CACHE_CONTROL = typename R_SURFACE_STATE::L1_CACHE_CONTROL;
    auto &productHelper = args.gmmHelper->getRootDeviceEnvironment().getHelper<ProductHelper>();

    auto cachePolicy = static_cast<L1_CACHE_CONTROL>(productHelper.getL1CachePolicy(args.isDebuggerActive));
    if (debugManager.flags.OverrideL1CacheControlInSurfaceState.get() != -1 &&
        debugManager.flags.ForceAllResourcesUncached.get() == false) {
        cachePolicy = static_cast<L1_CACHE_CONTROL>(debugManager.flags.OverrideL1CacheControlInSurfaceState.get());
    }
    surfaceState->setL1CacheControlCachePolicy(cachePolicy);
}

template <typename Family>
void EncodeEnableRayTracing<Family>::programEnableRayTracing(LinearStream &commandStream, uint64_t backBuffer) {
    auto cmd = Family::cmd3dStateBtd;
    cmd.setPerDssMemoryBackedBufferSize(static_cast<typename Family::_3DSTATE_BTD::PER_DSS_MEMORY_BACKED_BUFFER_SIZE>(RayTracingHelper::getMemoryBackedFifoSizeToPatch()));
    cmd.setMemoryBackedBufferBasePointer(backBuffer);
    append3dStateBtd(&cmd);
    *commandStream.getSpaceForCmd<typename Family::_3DSTATE_BTD>() = cmd;
}

template <typename Family>
inline void EncodeWA<Family>::setAdditionalPipeControlFlagsForNonPipelineStateCommand(PipeControlArgs &args) {
    args.unTypedDataPortCacheFlush = true;
}

} // namespace NEO
