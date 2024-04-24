/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/scratch_space_controller.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
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

#include <algorithm>
#include <type_traits>

namespace NEO {
constexpr size_t timestampDestinationAddressAlignment = 16;
constexpr size_t immWriteDestinationAddressAlignment = 8;

template <typename Family>
template <typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::setGrfInfo(InterfaceDescriptorType *pInterfaceDescriptor, uint32_t grfCount,
                                              const size_t &sizeCrossThreadData, const size_t &sizePerThreadData,
                                              const RootDeviceEnvironment &rootDeviceEnvironment) {
}

template <typename Family>
template <typename WalkerType>
void EncodeDispatchKernel<Family>::encode(CommandContainer &container, EncodeDispatchKernelArgs &args) {
    using SHARED_LOCAL_MEMORY_SIZE = typename WalkerType::InterfaceDescriptorType::SHARED_LOCAL_MEMORY_SIZE;
    using STATE_BASE_ADDRESS = typename Family::STATE_BASE_ADDRESS;
    using POSTSYNC_DATA = std::remove_reference_t<std::invoke_result_t<decltype(&WalkerType::getPostSync), WalkerType &>>;

    constexpr bool heaplessModeEnabled = Family::template isHeaplessMode<WalkerType>();
    const HardwareInfo &hwInfo = args.device->getHardwareInfo();
    auto &rootDeviceEnvironment = args.device->getRootDeviceEnvironment();

    const auto &kernelDescriptor = args.dispatchInterface->getKernelDescriptor();
    auto sizeCrossThreadData = args.dispatchInterface->getCrossThreadDataSize();
    auto sizePerThreadData = args.dispatchInterface->getPerThreadDataSize();
    auto sizePerThreadDataForWholeGroup = args.dispatchInterface->getPerThreadDataSizeForWholeThreadGroup();
    auto pImplicitArgs = args.dispatchInterface->getImplicitArgs();

    LinearStream *listCmdBufferStream = container.getCommandStream();

    auto threadDims = static_cast<const uint32_t *>(args.threadGroupDimensions);
    const Vec3<size_t> threadStartVec{0, 0, 0};
    Vec3<size_t> threadDimsVec{0, 0, 0};
    if (!args.isIndirect) {
        threadDimsVec = {threadDims[0], threadDims[1], threadDims[2]};
    }

    bool systolicModeRequired = kernelDescriptor.kernelAttributes.flags.usesSystolicPipelineSelectMode;
    if (container.systolicModeSupportRef() && (container.lastPipelineSelectModeRequiredRef() != systolicModeRequired)) {
        container.lastPipelineSelectModeRequiredRef() = systolicModeRequired;
        EncodeComputeMode<Family>::adjustPipelineSelect(container, kernelDescriptor);
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
                                                       kernelDescriptor.kernelAttributes.barrierCount,
                                                       hwInfo);

    auto &gfxCoreHelper = args.device->getGfxCoreHelper();
    auto slmSize = static_cast<uint32_t>(
        gfxCoreHelper.computeSlmValues(hwInfo, args.dispatchInterface->getSlmTotalSize()));

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

    if (sshProgrammingRequired) {
        bool isBindlessKernel = NEO::KernelDescriptor::isBindlessAddressingKernel(kernelDescriptor);
        if (isBindlessKernel) {
            bool globalBindlessSsh = args.device->getBindlessHeapsHelper() != nullptr;
            auto sshHeapSize = args.dispatchInterface->getSurfaceStateHeapDataSize();

            if (sshHeapSize > 0u) {
                auto ssh = args.surfaceStateHeap;
                if (ssh == nullptr) {
                    container.prepareBindfulSsh();
                    ssh = container.getHeapWithRequiredSizeAndAlignment(HeapType::surfaceState, sshHeapSize, BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE);
                }

                uint64_t bindlessSshBaseOffset = ptrDiff(ssh->getSpace(0), ssh->getCpuBase());
                if (globalBindlessSsh) {
                    bindlessSshBaseOffset += ptrDiff(ssh->getGraphicsAllocation()->getGpuAddress(), ssh->getGraphicsAllocation()->getGpuBaseAddress());
                }

                if constexpr (heaplessModeEnabled == false) {
                    if (bindingTableStateCount > 0u) {
                        auto bindingTablePointer = static_cast<uint32_t>(EncodeSurfaceState<Family>::pushBindingTableAndSurfaceStates(
                            *ssh,
                            args.dispatchInterface->getSurfaceStateHeapData(),
                            args.dispatchInterface->getSurfaceStateHeapDataSize(), bindingTableStateCount,
                            kernelDescriptor.payloadMappings.bindingTable.tableOffset));

                        idd.setBindingTablePointer(bindingTablePointer);
                    }
                }

                if (bindingTableStateCount == 0) {
                    // Allocate space for new ssh data
                    auto dstSurfaceState = ssh->getSpace(sshHeapSize);
                    memcpy_s(dstSurfaceState, sshHeapSize, args.dispatchInterface->getSurfaceStateHeapData(), sshHeapSize);
                }
                args.dispatchInterface->patchBindlessOffsetsInCrossThreadData(bindlessSshBaseOffset);
            }

        } else {
            if constexpr (heaplessModeEnabled == false) {
                container.prepareBindfulSsh();
                if (bindingTableStateCount > 0u) {
                    auto ssh = args.surfaceStateHeap;
                    if (ssh == nullptr) {
                        ssh = container.getHeapWithRequiredSizeAndAlignment(HeapType::surfaceState, args.dispatchInterface->getSurfaceStateHeapDataSize(), BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE);
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

    PreemptionHelper::programInterfaceDescriptorDataPreemption<Family>(&idd, args.preemptionMode);

    uint32_t samplerCount = 0;

    if constexpr (Family::supportsSampler && heaplessModeEnabled == false) {
        if (args.device->getDeviceInfo().imageSupport) {

            uint32_t samplerStateOffset = 0;

            if (kernelDescriptor.payloadMappings.samplerTable.numSamplers > 0) {
                auto dsHeap = args.dynamicStateHeap;
                if (dsHeap == nullptr) {
                    dsHeap = container.getIndirectHeap(HeapType::dynamicState);
                }
                UNRECOVERABLE_IF(!dsHeap);

                samplerCount = kernelDescriptor.payloadMappings.samplerTable.numSamplers;
                samplerStateOffset = EncodeStates<Family>::copySamplerState(
                    dsHeap, kernelDescriptor.payloadMappings.samplerTable.tableOffset,
                    kernelDescriptor.payloadMappings.samplerTable.numSamplers, kernelDescriptor.payloadMappings.samplerTable.borderColor,
                    args.dispatchInterface->getDynamicStateHeapData(),
                    args.device->getBindlessHeapsHelper(), rootDeviceEnvironment);
            }

            idd.setSamplerStatePointer(samplerStateOffset);
            args.dispatchInterface->patchSamplerBindlessOffsetsInCrossThreadData(samplerStateOffset);
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

    uint32_t sizeThreadData = sizePerThreadDataForWholeGroup + sizeCrossThreadData;
    uint32_t sizeForImplicitArgsPatching = NEO::ImplicitArgsHelper::getSizeForImplicitArgsPatching(pImplicitArgs, kernelDescriptor, !localIdsGenerationByRuntime, rootDeviceEnvironment);
    uint32_t iohRequiredSize = sizeThreadData + sizeForImplicitArgsPatching;
    {
        auto heap = container.getIndirectHeap(HeapType::indirectObject);
        UNRECOVERABLE_IF(!heap);
        heap->align(DefaultWalkerType::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);
        void *ptr = nullptr;
        if (args.isKernelDispatchedFromImmediateCmdList) {
            ptr = container.getHeapWithRequiredSizeAndAlignment(HeapType::indirectObject, iohRequiredSize, DefaultWalkerType::INDIRECTDATASTARTADDRESS_ALIGN_SIZE)->getSpace(iohRequiredSize);
        } else {
            ptr = container.getHeapSpaceAllowGrow(HeapType::indirectObject, iohRequiredSize);
        }
        UNRECOVERABLE_IF(!ptr);
        offsetThreadData = (is64bit ? heap->getHeapGpuStartOffset() : heap->getHeapGpuBase()) + static_cast<uint64_t>(heap->getUsed() - sizeThreadData);
        auto &rootDeviceEnvironment = args.device->getRootDeviceEnvironment();
        if (pImplicitArgs) {
            offsetThreadData -= ImplicitArgs::getSize();
            pImplicitArgs->localIdTablePtr = heap->getGraphicsAllocation()->getGpuAddress() + heap->getUsed() - iohRequiredSize;
            ptr = NEO::ImplicitArgsHelper::patchImplicitArgs(ptr, *pImplicitArgs, kernelDescriptor, std::make_pair(localIdsGenerationByRuntime, requiredWorkgroupOrder), rootDeviceEnvironment);
        }

        if (sizeCrossThreadData > 0) {
            memcpy_s(ptr, sizeCrossThreadData,
                     crossThreadData, sizeCrossThreadData);
        }

        if (args.isIndirect) {
            auto gpuPtr = heap->getGraphicsAllocation()->getGpuAddress() + static_cast<uint64_t>(heap->getUsed() - sizeThreadData - inlineDataProgrammingOffset);
            uint64_t implicitArgsGpuPtr = 0u;
            if (pImplicitArgs) {
                implicitArgsGpuPtr = gpuPtr + inlineDataProgrammingOffset - ImplicitArgs::getSize();
            }
            EncodeIndirectParams<Family>::encode(container, gpuPtr, args.dispatchInterface, implicitArgsGpuPtr);
        }

        auto perThreadDataPtr = args.dispatchInterface->getPerThreadData();
        if (perThreadDataPtr != nullptr) {
            ptr = ptrOffset(ptr, sizeCrossThreadData);
            memcpy_s(ptr, sizePerThreadDataForWholeGroup,
                     perThreadDataPtr, sizePerThreadDataForWholeGroup);
        }
    }

    if (args.isHeaplessStateInitEnabled == false) {
        if (container.isAnyHeapDirty() ||
            args.requiresUncachedMocs) {

            PipeControlArgs syncArgs;
            syncArgs.dcFlushEnable = args.dcFlushEnable;
            MemorySynchronizationCommands<Family>::addSingleBarrier(*container.getCommandStream(), syncArgs);
            STATE_BASE_ADDRESS sbaCmd;
            auto gmmHelper = container.getDevice()->getGmmHelper();
            uint32_t statelessMocsIndex =
                args.requiresUncachedMocs ? (gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) >> 1) : (gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) >> 1);
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
                heaplessModeEnabled,         // heaplessModeEnabled
            };
            EncodeStateBaseAddress<Family>::encode(encodeStateBaseAddressArgs);
            container.setDirtyStateForAllHeaps(false);
        }
    }

    if (NEO::PauseOnGpuProperties::pauseModeAllowed(NEO::debugManager.flags.PauseOnEnqueue.get(), args.device->debugExecutionCounter.load(), NEO::PauseOnGpuProperties::PauseMode::BeforeWorkload)) {
        void *commandBuffer = listCmdBufferStream->getSpace(MemorySynchronizationCommands<Family>::getSizeForBarrierWithPostSyncOperation(rootDeviceEnvironment, false));
        args.additionalCommands->push_back(commandBuffer);

        EncodeSemaphore<Family>::applyMiSemaphoreWaitCommand(*listCmdBufferStream, *args.additionalCommands);
    }

    uint8_t *inlineData = reinterpret_cast<uint8_t *>(walkerCmd.getInlineDataPointer());
    EncodeDispatchKernel<Family>::programInlineDataHeapless<heaplessModeEnabled>(inlineData, args, container, offsetThreadData);

    if constexpr (heaplessModeEnabled == false) {
        walkerCmd.setIndirectDataStartAddress(static_cast<uint32_t>(offsetThreadData));
        walkerCmd.setIndirectDataLength(sizeThreadData);

        container.getIndirectHeap(HeapType::indirectObject)->align(rootDeviceEnvironment.getHelper<GfxCoreHelper>().getIOHAlignment());
    }

    EncodeDispatchKernel<Family>::encodeThreadData(walkerCmd,
                                                   nullptr,
                                                   threadDims,
                                                   args.dispatchInterface->getGroupSize(),
                                                   kernelDescriptor.kernelAttributes.simdSize,
                                                   kernelDescriptor.kernelAttributes.numLocalIdChannels,
                                                   args.dispatchInterface->getNumThreadsPerThreadGroup(),
                                                   args.dispatchInterface->getThreadExecutionMask(),
                                                   localIdsGenerationByRuntime,
                                                   inlineDataProgramming,
                                                   args.isIndirect,
                                                   requiredWorkgroupOrder,
                                                   rootDeviceEnvironment);

    if (args.inOrderExecInfo) {
        EncodeDispatchKernel<Family>::setupPostSyncForInOrderExec<WalkerType>(walkerCmd, args);
    } else if (args.eventAddress) {
        EncodeDispatchKernel<Family>::setupPostSyncForRegularEvent<WalkerType>(walkerCmd, args);
    }

    if (debugManager.flags.ForceComputeWalkerPostSyncFlush.get() == 1) {
        auto &postSync = walkerCmd.getPostSync();
        postSync.setDataportPipelineFlush(true);
        postSync.setDataportSubsliceCacheFlush(true);
    }

    walkerCmd.setPredicateEnable(args.isPredicate);

    auto threadGroupCount = walkerCmd.getThreadGroupIdXDimension() * walkerCmd.getThreadGroupIdYDimension() * walkerCmd.getThreadGroupIdZDimension();
    EncodeDispatchKernel<Family>::adjustInterfaceDescriptorData(idd, *args.device, hwInfo, threadGroupCount, kernelDescriptor.kernelAttributes.numGrfRequired, walkerCmd);
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

    EncodeDispatchKernel<Family>::appendAdditionalIDDFields(&idd, rootDeviceEnvironment, threadsPerThreadGroup,
                                                            args.dispatchInterface->getSlmTotalSize(),
                                                            args.dispatchInterface->getSlmPolicy());

    EncodeWalkerArgs walkerArgs{
        args.isCooperative ? KernelExecutionType::concurrent : KernelExecutionType::defaultType,
        args.requiresSystemMemoryFence(),
        kernelDescriptor,
        args.requiredDispatchWalkOrder,
        args.additionalSizeParam,
        args.device->getDeviceInfo().maxFrontEndThreads};
    EncodeDispatchKernel<Family>::encodeAdditionalWalkerFields(rootDeviceEnvironment, walkerCmd, walkerArgs);

    PreemptionHelper::applyPreemptionWaCmdsBegin<Family>(listCmdBufferStream, *args.device);

    if (args.partitionCount > 1 && !args.isInternal) {
        const uint64_t workPartitionAllocationGpuVa = args.device->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocationGpuAddress();

        ImplicitScalingDispatch<Family>::dispatchCommands(*listCmdBufferStream,
                                                          walkerCmd,
                                                          &args.outWalkerPtr,
                                                          args.device->getDeviceBitfield(),
                                                          args.requiredPartitionDim,
                                                          args.partitionCount,
                                                          !(container.getFlushTaskUsedForImmediate() || container.isUsingPrimaryBuffer()),
                                                          !args.isKernelDispatchedFromImmediateCmdList,
                                                          args.dcFlushEnable,
                                                          gfxCoreHelper.singleTileExecImplicitScalingRequired(args.isCooperative),
                                                          workPartitionAllocationGpuVa,
                                                          hwInfo);
    } else {
        args.partitionCount = 1;
        auto buffer = listCmdBufferStream->getSpaceForCmd<WalkerType>();
        args.outWalkerPtr = buffer;
        *buffer = walkerCmd;
    }

    if (args.cpuWalkerBuffer) {
        *reinterpret_cast<WalkerType *>(args.cpuWalkerBuffer) = walkerCmd;
    }

    PreemptionHelper::applyPreemptionWaCmdsEnd<Family>(listCmdBufferStream, *args.device);

    if (NEO::PauseOnGpuProperties::pauseModeAllowed(NEO::debugManager.flags.PauseOnEnqueue.get(), args.device->debugExecutionCounter.load(), NEO::PauseOnGpuProperties::PauseMode::AfterWorkload)) {
        void *commandBuffer = listCmdBufferStream->getSpace(MemorySynchronizationCommands<Family>::getSizeForBarrierWithPostSyncOperation(rootDeviceEnvironment, false));
        args.additionalCommands->push_back(commandBuffer);

        EncodeSemaphore<Family>::applyMiSemaphoreWaitCommand(*listCmdBufferStream, *args.additionalCommands);
    }
}

template <typename Family>
template <typename WalkerType>
void EncodeDispatchKernel<Family>::setupPostSyncForRegularEvent(WalkerType &walkerCmd, const EncodeDispatchKernelArgs &args) {
    using POSTSYNC_DATA = std::remove_reference_t<std::invoke_result_t<decltype(&WalkerType::getPostSync), WalkerType &>>;

    auto &postSync = walkerCmd.getPostSync();

    postSync.setDataportPipelineFlush(true);
    postSync.setDataportSubsliceCacheFlush(true);

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

    postSync.setOperation(operationType);
    postSync.setImmediateData(immData);
    postSync.setDestinationAddress(gpuVa);

    EncodeDispatchKernel<Family>::setupPostSyncMocs(walkerCmd, args.device->getRootDeviceEnvironment(), args.dcFlushEnable);
    EncodeDispatchKernel<Family>::adjustTimestampPacket(walkerCmd, args);
}

template <typename Family>
template <typename WalkerType>
void EncodeDispatchKernel<Family>::setupPostSyncForInOrderExec(WalkerType &walkerCmd, const EncodeDispatchKernelArgs &args) {
    using POSTSYNC_DATA = std::remove_reference_t<std::invoke_result_t<decltype(&WalkerType::getPostSync), WalkerType &>>;

    auto &postSync = walkerCmd.getPostSync();

    postSync.setDataportPipelineFlush(true);
    postSync.setDataportSubsliceCacheFlush(true);

    uint64_t gpuVa = args.inOrderExecInfo->getBaseDeviceAddress() + args.inOrderExecInfo->getAllocationOffset();

    UNRECOVERABLE_IF(!(isAligned<immWriteDestinationAddressAlignment>(gpuVa)));

    postSync.setOperation(POSTSYNC_DATA::OPERATION_WRITE_IMMEDIATE_DATA);
    postSync.setImmediateData(args.inOrderCounterValue);
    postSync.setDestinationAddress(gpuVa);

    EncodeDispatchKernel<Family>::setupPostSyncMocs(walkerCmd, args.device->getRootDeviceEnvironment(), args.dcFlushEnable);
    EncodeDispatchKernel<Family>::adjustTimestampPacket(walkerCmd, args);
}

template <typename Family>
template <typename WalkerType>
inline void EncodeDispatchKernel<Family>::setupPostSyncMocs(WalkerType &walkerCmd, const RootDeviceEnvironment &rootDeviceEnvironment, bool dcFlush) {
    auto &postSyncData = walkerCmd.getPostSync();
    auto gmmHelper = rootDeviceEnvironment.getGmmHelper();

    if (dcFlush) {
        postSyncData.setMocs(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED));
    } else {
        postSyncData.setMocs(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER));
    }

    if (debugManager.flags.OverridePostSyncMocs.get() != -1) {
        postSyncData.setMocs(debugManager.flags.OverridePostSyncMocs.get());
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
        UNRECOVERABLE_IF(localIdDimensions != 3);
        uint32_t emitLocalIdsForDim = (1 << 0) | (1 << 1) | (1 << 2);
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
}

template <typename Family>
void EncodeComputeMode<Family>::adjustPipelineSelect(CommandContainer &container, const NEO::KernelDescriptor &kernelDescriptor) {

    PipelineSelectArgs pipelineSelectArgs;
    pipelineSelectArgs.systolicPipelineSelectMode = kernelDescriptor.kernelAttributes.flags.usesSystolicPipelineSelectMode;
    pipelineSelectArgs.systolicPipelineSelectSupport = container.systolicModeSupportRef();

    PreambleHelper<Family>::programPipelineSelect(container.getCommandStream(),
                                                  pipelineSelectArgs,
                                                  container.getDevice()->getRootDeviceEnvironment());
}

template <typename Family>
inline void EncodeMediaInterfaceDescriptorLoad<Family>::encode(CommandContainer &container, IndirectHeap *childDsh) {
}

template <typename Family>
void EncodeMiFlushDW<Family>::adjust(MI_FLUSH_DW *miFlushDwCmd, const ProductHelper &productHelper) {
    miFlushDwCmd->setFlushCcs(1);
    miFlushDwCmd->setFlushLlc(1);
}

template <typename Family>
bool EncodeSurfaceState<Family>::isBindingTablePrefetchPreferred() {
    return false;
}

template <typename Family>
void EncodeSurfaceState<Family>::encodeExtraBufferParams(EncodeSurfaceStateArgs &args) {
    auto surfaceState = reinterpret_cast<R_SURFACE_STATE *>(args.outMemory);
    Gmm *gmm = args.allocation ? args.allocation->getDefaultGmm() : nullptr;
    uint32_t compressionFormat = 0;

    bool setConstCachePolicy = false;
    if (args.allocation && args.allocation->getAllocationType() == AllocationType::constantSurface) {
        setConstCachePolicy = true;
    }

    if (surfaceState->getMemoryObjectControlState() == args.gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) &&
        debugManager.flags.ForceL1Caching.get() != 0) {
        setConstCachePolicy = true;
    }

    if (setConstCachePolicy == true) {
        surfaceState->setMemoryObjectControlState(args.gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST));
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
void EncodeSurfaceState<Family>::setCoherencyType(R_SURFACE_STATE *surfaceState, COHERENCY_TYPE coherencyType) {
    surfaceState->setCoherencyType(R_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT);
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
void EncodeWA<Family>::adjustCompressionFormatForPlanarImage(uint32_t &compressionFormat, int plane) {
    static_assert(sizeof(plane) == sizeof(GMM_YUV_PLANE_ENUM));
    if (plane == GMM_PLANE_Y) {
        compressionFormat &= 0xf;
    } else if ((plane == GMM_PLANE_U) || (plane == GMM_PLANE_V)) {
        compressionFormat |= 0x10;
    }
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
template <typename WalkerType>
void EncodeDispatchKernel<Family>::adjustWalkOrder(WalkerType &walkerCmd, uint32_t requiredWorkGroupOrder, const RootDeviceEnvironment &rootDeviceEnvironment) {}

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
size_t EncodeStates<Family>::getSshHeapSize() {
    return 2 * MemoryConstants::megaByte;
}

template <typename Family>
void InOrderPatchCommandHelpers::PatchCmd<Family>::patchComputeWalker(uint64_t appendCounterValue) {
    auto walkerCmd = reinterpret_cast<typename Family::DefaultWalkerType *>(cmd1);
    auto &postSync = walkerCmd->getPostSync();
    postSync.setImmediateData(baseCounterValue + appendCounterValue);
}

} // namespace NEO
