/*
 * Copyright (C) 2020-2022 Intel Corporation
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
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_walk_order.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/pipeline_select_helper.h"
#include "shared/source/helpers/ray_tracing_helper.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"
#include "shared/source/kernel/implicit_args.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/os_interface/hw_info_config.h"

#include <algorithm>

namespace NEO {
constexpr size_t TimestampDestinationAddressAlignment = 16;

template <typename Family>
void EncodeDispatchKernel<Family>::setGrfInfo(INTERFACE_DESCRIPTOR_DATA *pInterfaceDescriptor, uint32_t numGrf,
                                              const size_t &sizeCrossThreadData, const size_t &sizePerThreadData,
                                              const HardwareInfo &hwInfo) {
}

template <typename Family>
void EncodeDispatchKernel<Family>::encode(CommandContainer &container,
                                          EncodeDispatchKernelArgs &args) {
    using SHARED_LOCAL_MEMORY_SIZE = typename Family::INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE;
    using STATE_BASE_ADDRESS = typename Family::STATE_BASE_ADDRESS;
    using INLINE_DATA = typename Family::INLINE_DATA;

    const HardwareInfo &hwInfo = args.device->getHardwareInfo();

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

    bool specialModeRequired = kernelDescriptor.kernelAttributes.flags.usesSpecialPipelineSelectMode;
    if (PreambleHelper<Family>::isSpecialPipelineSelectModeChanged(container.lastPipelineSelectModeRequired, specialModeRequired, hwInfo)) {
        container.lastPipelineSelectModeRequired = specialModeRequired;
        EncodeComputeMode<Family>::adjustPipelineSelect(container, kernelDescriptor);
    }

    WALKER_TYPE walkerCmd = Family::cmdInitGpgpuWalker;
    auto &idd = walkerCmd.getInterfaceDescriptor();

    EncodeDispatchKernel<Family>::setGrfInfo(&idd, kernelDescriptor.kernelAttributes.numGrfRequired, sizeCrossThreadData,
                                             sizePerThreadData, hwInfo);
    auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    hwInfoConfig.updateIddCommand(&idd, kernelDescriptor.kernelAttributes.numGrfRequired,
                                  args.dispatchInterface->getSchedulingHintExp());

    bool localIdsGenerationByRuntime = args.dispatchInterface->requiresGenerationOfLocalIdsByRuntime();
    auto requiredWorkgroupOrder = args.dispatchInterface->getRequiredWorkgroupOrder();
    bool inlineDataProgramming = EncodeDispatchKernel<Family>::inlineDataProgrammingRequired(kernelDescriptor);
    {
        auto alloc = args.dispatchInterface->getIsaAllocation();
        UNRECOVERABLE_IF(nullptr == alloc);
        auto offset = alloc->getGpuAddressToPatch();
        if (!localIdsGenerationByRuntime) {
            offset += kernelDescriptor.entryPoints.skipPerThreadDataLoad;
        }
        idd.setKernelStartPointer(offset);
    }

    auto threadsPerThreadGroup = args.dispatchInterface->getNumThreadsPerThreadGroup();
    idd.setNumberOfThreadsInGpgpuThreadGroup(threadsPerThreadGroup);

    EncodeDispatchKernel<Family>::programBarrierEnable(idd,
                                                       kernelDescriptor.kernelAttributes.barrierCount,
                                                       hwInfo);

    auto slmSize = static_cast<SHARED_LOCAL_MEMORY_SIZE>(
        HwHelperHw<Family>::get().computeSlmValues(hwInfo, args.dispatchInterface->getSlmTotalSize()));

    if (DebugManager.flags.OverrideSlmAllocationSize.get() != -1) {
        slmSize = static_cast<SHARED_LOCAL_MEMORY_SIZE>(DebugManager.flags.OverrideSlmAllocationSize.get());
    }
    idd.setSharedLocalMemorySize(slmSize);

    auto bindingTableStateCount = kernelDescriptor.payloadMappings.bindingTable.numEntries;
    uint32_t bindingTablePointer = 0u;
    if ((kernelDescriptor.kernelAttributes.bufferAddressingMode == KernelDescriptor::BindfulAndStateless) ||
        kernelDescriptor.kernelAttributes.flags.usesImages) {
        container.prepareBindfulSsh();
        if (bindingTableStateCount > 0u) {
            auto ssh = container.getHeapWithRequiredSizeAndAlignment(HeapType::SURFACE_STATE, args.dispatchInterface->getSurfaceStateHeapDataSize(), BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE);
            bindingTablePointer = static_cast<uint32_t>(EncodeSurfaceState<Family>::pushBindingTableAndSurfaceStates(
                *ssh, bindingTableStateCount,
                args.dispatchInterface->getSurfaceStateHeapData(),
                args.dispatchInterface->getSurfaceStateHeapDataSize(), bindingTableStateCount,
                kernelDescriptor.payloadMappings.bindingTable.tableOffset));
        }
    }
    idd.setBindingTablePointer(bindingTablePointer);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<Family>(&idd, args.preemptionMode);

    if constexpr (Family::supportsSampler) {
        auto heap = ApiSpecificConfig::getBindlessConfiguration() ? args.device->getBindlessHeapsHelper()->getHeap(BindlessHeapsHelper::GLOBAL_DSH) : container.getIndirectHeap(HeapType::DYNAMIC_STATE);
        UNRECOVERABLE_IF(!heap);

        uint32_t samplerStateOffset = 0;
        uint32_t samplerCount = 0;

        if (kernelDescriptor.payloadMappings.samplerTable.numSamplers > 0) {
            samplerCount = kernelDescriptor.payloadMappings.samplerTable.numSamplers;
            samplerStateOffset = EncodeStates<Family>::copySamplerState(
                heap, kernelDescriptor.payloadMappings.samplerTable.tableOffset,
                kernelDescriptor.payloadMappings.samplerTable.numSamplers, kernelDescriptor.payloadMappings.samplerTable.borderColor,
                args.dispatchInterface->getDynamicStateHeapData(),
                args.device->getBindlessHeapsHelper(), hwInfo);
            if (ApiSpecificConfig::getBindlessConfiguration()) {
                container.getResidencyContainer().push_back(args.device->getBindlessHeapsHelper()->getHeap(NEO::BindlessHeapsHelper::BindlesHeapType::GLOBAL_DSH)->getGraphicsAllocation());
            }
        }

        idd.setSamplerStatePointer(samplerStateOffset);
        EncodeDispatchKernel<Family>::adjustBindingTablePrefetch(idd, samplerCount, bindingTableStateCount);
    } else {
        EncodeDispatchKernel<Family>::adjustBindingTablePrefetch(idd, 0u, bindingTableStateCount);
    }

    uint64_t offsetThreadData = 0u;
    const uint32_t inlineDataSize = sizeof(INLINE_DATA);
    auto crossThreadData = args.dispatchInterface->getCrossThreadData();

    uint32_t inlineDataProgrammingOffset = 0u;

    if (inlineDataProgramming) {
        inlineDataProgrammingOffset = std::min(inlineDataSize, sizeCrossThreadData);
        auto dest = reinterpret_cast<char *>(walkerCmd.getInlineDataPointer());
        memcpy_s(dest, inlineDataProgrammingOffset, crossThreadData, inlineDataProgrammingOffset);
        sizeCrossThreadData -= inlineDataProgrammingOffset;
        crossThreadData = ptrOffset(crossThreadData, inlineDataProgrammingOffset);
        inlineDataProgramming = inlineDataProgrammingOffset != 0;
    }

    uint32_t sizeThreadData = sizePerThreadDataForWholeGroup + sizeCrossThreadData;
    uint32_t sizeForImplicitArgsPatching = NEO::ImplicitArgsHelper::getSizeForImplicitArgsPatching(pImplicitArgs, kernelDescriptor, hwInfo);
    uint32_t iohRequiredSize = sizeThreadData + sizeForImplicitArgsPatching;
    {
        auto heap = container.getIndirectHeap(HeapType::INDIRECT_OBJECT);
        UNRECOVERABLE_IF(!heap);
        heap->align(WALKER_TYPE::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);

        auto ptr = container.getHeapSpaceAllowGrow(HeapType::INDIRECT_OBJECT, iohRequiredSize);
        UNRECOVERABLE_IF(!ptr);
        offsetThreadData = (is64bit ? heap->getHeapGpuStartOffset() : heap->getHeapGpuBase()) + static_cast<uint64_t>(heap->getUsed() - sizeThreadData);

        if (pImplicitArgs) {
            offsetThreadData -= sizeof(ImplicitArgs);
            pImplicitArgs->localIdTablePtr = heap->getGraphicsAllocation()->getGpuAddress() + heap->getUsed() - iohRequiredSize;
            ptr = NEO::ImplicitArgsHelper::patchImplicitArgs(ptr, *pImplicitArgs, kernelDescriptor, hwInfo, std::make_pair(localIdsGenerationByRuntime, requiredWorkgroupOrder));
        }

        if (sizeCrossThreadData > 0) {
            memcpy_s(ptr, sizeCrossThreadData,
                     crossThreadData, sizeCrossThreadData);
        }
        if (args.isIndirect) {
            auto gpuPtr = heap->getGraphicsAllocation()->getGpuAddress() + static_cast<uint64_t>(heap->getUsed() - sizeThreadData - inlineDataProgrammingOffset);
            uint64_t implicitArgsGpuPtr = 0u;
            if (pImplicitArgs) {
                implicitArgsGpuPtr = gpuPtr + inlineDataProgrammingOffset - sizeof(ImplicitArgs);
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

    if (shouldUpdateGlobalAtomics(container.lastSentUseGlobalAtomics, args.useGlobalAtomics, args.partitionCount > 1) ||
        container.isAnyHeapDirty() ||
        args.requiresUncachedMocs) {

        PipeControlArgs syncArgs;
        syncArgs.dcFlushEnable = MemorySynchronizationCommands<Family>::getDcFlushEnable(true, hwInfo);
        MemorySynchronizationCommands<Family>::addSingleBarrier(*container.getCommandStream(), syncArgs);
        STATE_BASE_ADDRESS sbaCmd;
        auto gmmHelper = container.getDevice()->getGmmHelper();
        uint32_t statelessMocsIndex =
            args.requiresUncachedMocs ? (gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) >> 1) : (gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) >> 1);
        EncodeStateBaseAddress<Family>::encode(container, sbaCmd, statelessMocsIndex, args.useGlobalAtomics, args.partitionCount > 1);
        container.setDirtyStateForAllHeaps(false);
        args.requiresUncachedMocs = false;
    }

    walkerCmd.setIndirectDataStartAddress(static_cast<uint32_t>(offsetThreadData));
    walkerCmd.setIndirectDataLength(sizeThreadData);

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
                                                   hwInfo);

    using POSTSYNC_DATA = typename Family::POSTSYNC_DATA;
    auto &postSync = walkerCmd.getPostSync();
    if (args.eventAddress != 0) {
        postSync.setDataportPipelineFlush(true);
        if (args.isTimestampEvent) {
            postSync.setOperation(POSTSYNC_DATA::OPERATION_WRITE_TIMESTAMP);
        } else {
            uint32_t stateSignaled = 0u;
            postSync.setOperation(POSTSYNC_DATA::OPERATION_WRITE_IMMEDIATE_DATA);
            postSync.setImmediateData(stateSignaled);
        }
        UNRECOVERABLE_IF(!(isAligned<TimestampDestinationAddressAlignment>(args.eventAddress)));
        postSync.setDestinationAddress(args.eventAddress);

        EncodeDispatchKernel<Family>::setupPostSyncMocs(walkerCmd, args.device->getRootDeviceEnvironment());
        EncodeDispatchKernel<Family>::adjustTimestampPacket(walkerCmd, hwInfo);
    }

    walkerCmd.setPredicateEnable(args.isPredicate);

    EncodeDispatchKernel<Family>::adjustInterfaceDescriptorData(idd, hwInfo);

    EncodeDispatchKernel<Family>::appendAdditionalIDDFields(&idd, hwInfo, threadsPerThreadGroup,
                                                            args.dispatchInterface->getSlmTotalSize(),
                                                            args.dispatchInterface->getSlmPolicy());

    EncodeWalkerArgs walkerArgs{
        args.isCooperative ? KernelExecutionType::Concurrent : KernelExecutionType::Default,
        args.isHostScopeSignalEvent && args.isKernelUsingSystemAllocation};
    EncodeDispatchKernel<Family>::encodeAdditionalWalkerFields(hwInfo, walkerCmd, walkerArgs);

    PreemptionHelper::applyPreemptionWaCmdsBegin<Family>(listCmdBufferStream, *args.device);

    if ((args.partitionCount > 1 && !args.isCooperative) &&
        !args.isInternal) {
        const uint64_t workPartitionAllocationGpuVa = args.device->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocationGpuAddress();
        if (args.eventAddress != 0) {
            postSync.setOperation(POSTSYNC_DATA::OPERATION_WRITE_TIMESTAMP);
        }
        ImplicitScalingDispatch<Family>::dispatchCommands(*listCmdBufferStream,
                                                          walkerCmd,
                                                          args.device->getDeviceBitfield(),
                                                          args.partitionCount,
                                                          !container.getFlushTaskUsedForImmediate(),
                                                          !args.isKernelDispatchedFromImmediateCmdList,
                                                          false,
                                                          workPartitionAllocationGpuVa,
                                                          hwInfo);
    } else {
        args.partitionCount = 1;
        auto buffer = listCmdBufferStream->getSpace(sizeof(walkerCmd));
        *(decltype(walkerCmd) *)buffer = walkerCmd;
    }

    PreemptionHelper::applyPreemptionWaCmdsEnd<Family>(listCmdBufferStream, *args.device);
}

template <typename Family>
inline void EncodeDispatchKernel<Family>::setupPostSyncMocs(WALKER_TYPE &walkerCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {
    auto &postSyncData = walkerCmd.getPostSync();
    auto gmmHelper = rootDeviceEnvironment.getGmmHelper();

    const auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    if (MemorySynchronizationCommands<Family>::getDcFlushEnable(true, hwInfo)) {
        postSyncData.setMocs(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED));
    } else {
        postSyncData.setMocs(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER));
    }

    if (DebugManager.flags.OverridePostSyncMocs.get() != -1) {
        postSyncData.setMocs(DebugManager.flags.OverridePostSyncMocs.get());
    }
}

template <typename Family>
inline void EncodeDispatchKernel<Family>::encodeAdditionalWalkerFields(const HardwareInfo &hwInfo, WALKER_TYPE &walkerCmd, const EncodeWalkerArgs &walkerArgs) {
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
    if (DebugManager.flags.EnableHwGenerationLocalIds.get() != -1) {
        hwGenerationOfLocalIdsEnabled = !!DebugManager.flags.EnableHwGenerationLocalIds.get();
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
                };
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
void EncodeDispatchKernel<Family>::encodeThreadData(WALKER_TYPE &walkerCmd,
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
                                                    const HardwareInfo &hwInfo) {

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
            executionMask = maxNBitValue((simd == 1) ? 32 : simd);
        }
    }

    walkerCmd.setExecutionMask(static_cast<uint32_t>(executionMask));
    walkerCmd.setSimdSize(getSimdConfig<WALKER_TYPE>(simd));

    walkerCmd.setMessageSimd(walkerCmd.getSimdSize());

    if (DebugManager.flags.ForceSimdMessageSizeInWalker.get() != -1) {
        walkerCmd.setMessageSimd(DebugManager.flags.ForceSimdMessageSizeInWalker.get());
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
    adjustWalkOrder(walkerCmd, requiredWorkGroupOrder, hwInfo);
    if (inlineDataProgrammingRequired == true) {
        walkerCmd.setEmitInlineParameter(1);
    }
}

template <typename Family>
void EncodeStateBaseAddress<Family>::setSbaAddressesForDebugger(NEO::Debugger::SbaAddresses &sbaAddress, const STATE_BASE_ADDRESS &sbaCmd) {
    sbaAddress.BindlessSurfaceStateBaseAddress = sbaCmd.getBindlessSurfaceStateBaseAddress();
    sbaAddress.DynamicStateBaseAddress = sbaCmd.getDynamicStateBaseAddress();
    sbaAddress.GeneralStateBaseAddress = sbaCmd.getGeneralStateBaseAddress();
    sbaAddress.InstructionBaseAddress = sbaCmd.getInstructionBaseAddress();
    sbaAddress.SurfaceStateBaseAddress = sbaCmd.getSurfaceStateBaseAddress();
    sbaAddress.IndirectObjectBaseAddress = 0;
}

template <typename Family>
void EncodeStateBaseAddress<Family>::encode(CommandContainer &container, STATE_BASE_ADDRESS &sbaCmd, bool multiOsContextCapable) {
    auto gmmHelper = container.getDevice()->getRootDeviceEnvironment().getGmmHelper();
    uint32_t statelessMocsIndex = (gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) >> 1);
    EncodeStateBaseAddress<Family>::encode(container, sbaCmd, statelessMocsIndex, false, multiOsContextCapable);
}

template <typename Family>
void EncodeStateBaseAddress<Family>::encode(CommandContainer &container, STATE_BASE_ADDRESS &sbaCmd, uint32_t statelessMocsIndex, bool useGlobalAtomics, bool multiOsContextCapable) {
    auto gmmHelper = container.getDevice()->getRootDeviceEnvironment().getGmmHelper();

    StateBaseAddressHelper<Family>::programStateBaseAddress(
        &sbaCmd,
        container.isHeapDirty(HeapType::DYNAMIC_STATE) ? container.getIndirectHeap(HeapType::DYNAMIC_STATE) : nullptr,
        container.isHeapDirty(HeapType::INDIRECT_OBJECT) ? container.getIndirectHeap(HeapType::INDIRECT_OBJECT) : nullptr,
        container.isHeapDirty(HeapType::SURFACE_STATE) ? container.getIndirectHeap(HeapType::SURFACE_STATE) : nullptr,
        0,
        true,
        statelessMocsIndex,
        container.getIndirectObjectHeapBaseAddress(),
        container.getInstructionHeapBaseAddress(),
        0,
        true,
        false,
        gmmHelper,
        multiOsContextCapable,
        MemoryCompressionState::NotApplicable,
        useGlobalAtomics,
        1u,
        nullptr);

    auto pCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(container.getCommandStream()->getSpace(sizeof(STATE_BASE_ADDRESS)));
    *pCmd = sbaCmd;

    auto &hwInfo = container.getDevice()->getHardwareInfo();
    auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    if (hwInfoConfig.isAdditionalStateBaseAddressWARequired(hwInfo)) {
        pCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(container.getCommandStream()->getSpace(sizeof(STATE_BASE_ADDRESS)));
        *pCmd = sbaCmd;
    }

    if (container.isHeapDirty(HeapType::SURFACE_STATE)) {
        auto heap = container.getIndirectHeap(HeapType::SURFACE_STATE);
        auto cmd = Family::cmdInitStateBindingTablePoolAlloc;
        cmd.setBindingTablePoolBaseAddress(heap->getHeapGpuBase());
        cmd.setBindingTablePoolBufferSize(heap->getHeapSizeInPages());
        cmd.setSurfaceObjectControlStateIndexToMocsTables(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER));

        auto buffer = container.getCommandStream()->getSpace(sizeof(cmd));
        *(typename Family::_3DSTATE_BINDING_TABLE_POOL_ALLOC *)buffer = cmd;
    }
}

template <typename Family>
size_t EncodeStateBaseAddress<Family>::getRequiredSizeForStateBaseAddress(Device &device, CommandContainer &container) {
    auto &hwInfo = device.getHardwareInfo();
    auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

    size_t size = sizeof(typename Family::STATE_BASE_ADDRESS);
    if (hwInfoConfig.isAdditionalStateBaseAddressWARequired(hwInfo)) {
        size += sizeof(typename Family::STATE_BASE_ADDRESS);
    }

    if (container.isHeapDirty(HeapType::SURFACE_STATE)) {
        size += sizeof(typename Family::_3DSTATE_BINDING_TABLE_POOL_ALLOC);
    }

    return size;
}

template <typename Family>
void EncodeComputeMode<Family>::programComputeModeCommand(LinearStream &csr, StateComputeModeProperties &properties, const HardwareInfo &hwInfo, LogicalStateHelper *logicalStateHelper) {
    using STATE_COMPUTE_MODE = typename Family::STATE_COMPUTE_MODE;
    using FORCE_NON_COHERENT = typename STATE_COMPUTE_MODE::FORCE_NON_COHERENT;

    STATE_COMPUTE_MODE stateComputeMode = Family::cmdInitStateComputeMode;
    auto maskBits = stateComputeMode.getMaskBits();

    FORCE_NON_COHERENT coherencyValue = (properties.isCoherencyRequired.value == 1) ? FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_DISABLED
                                                                                    : FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT;
    stateComputeMode.setForceNonCoherent(coherencyValue);
    maskBits |= Family::stateComputeModeForceNonCoherentMask;

    stateComputeMode.setLargeGrfMode(properties.largeGrfMode.value == 1);
    maskBits |= Family::stateComputeModeLargeGrfModeMask;

    if (DebugManager.flags.ForceMultiGpuAtomics.get() != -1) {
        stateComputeMode.setForceDisableSupportForMultiGpuAtomics(!!DebugManager.flags.ForceMultiGpuAtomics.get());
        maskBits |= Family::stateComputeModeForceDisableSupportMultiGpuAtomics;
    }

    if (DebugManager.flags.ForceMultiGpuPartialWrites.get() != -1) {
        stateComputeMode.setForceDisableSupportForMultiGpuPartialWrites(!!DebugManager.flags.ForceMultiGpuPartialWrites.get());
        maskBits |= Family::stateComputeModeForceDisableSupportMultiGpuPartialWrites;
    }

    stateComputeMode.setMaskBits(maskBits);

    auto buffer = csr.getSpaceForCmd<STATE_COMPUTE_MODE>();
    *buffer = stateComputeMode;
}

template <typename Family>
void EncodeComputeMode<Family>::adjustPipelineSelect(CommandContainer &container, const NEO::KernelDescriptor &kernelDescriptor) {
    using PIPELINE_SELECT = typename Family::PIPELINE_SELECT;
    auto pipelineSelectCmd = Family::cmdInitPipelineSelect;
    auto isSpecialModeSelected = kernelDescriptor.kernelAttributes.flags.usesSpecialPipelineSelectMode;

    PreambleHelper<Family>::appendProgramPipelineSelect(&pipelineSelectCmd, isSpecialModeSelected, container.getDevice()->getHardwareInfo());

    pipelineSelectCmd.setPipelineSelection(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);

    auto buffer = container.getCommandStream()->getSpace(sizeof(pipelineSelectCmd));
    *(decltype(pipelineSelectCmd) *)buffer = pipelineSelectCmd;
}

template <typename Family>
inline void EncodeMediaInterfaceDescriptorLoad<Family>::encode(CommandContainer &container) {
}

template <typename Family>
void EncodeMiFlushDW<Family>::appendMiFlushDw(MI_FLUSH_DW *miFlushDwCmd, const HardwareInfo &hwInfo) {
    miFlushDwCmd->setFlushCcs(1);
    miFlushDwCmd->setFlushLlc(1);
}

template <typename Family>
void EncodeMiFlushDW<Family>::programMiFlushDwWA(LinearStream &commandStream) {
    auto miFlushDwCmd = commandStream.getSpaceForCmd<MI_FLUSH_DW>();
    *miFlushDwCmd = Family::cmdInitMiFlushDw;
}

template <typename Family>
size_t EncodeMiFlushDW<Family>::getMiFlushDwWaSize() {
    return sizeof(typename Family::MI_FLUSH_DW);
}

template <typename Family>
bool EncodeSurfaceState<Family>::doBindingTablePrefetch() {
    return false;
}

template <typename Family>
void EncodeSurfaceState<Family>::encodeExtraBufferParams(EncodeSurfaceStateArgs &args) {
    auto surfaceState = reinterpret_cast<R_SURFACE_STATE *>(args.outMemory);
    Gmm *gmm = args.allocation ? args.allocation->getDefaultGmm() : nullptr;
    uint32_t compressionFormat = 0;

    bool setConstCachePolicy = false;
    if (args.allocation && args.allocation->getAllocationType() == AllocationType::CONSTANT_SURFACE) {
        setConstCachePolicy = true;
    }

    if (surfaceState->getMemoryObjectControlState() == args.gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) &&
        DebugManager.flags.ForceL1Caching.get() != 0) {
        setConstCachePolicy = true;
    }

    if (setConstCachePolicy == true) {
        surfaceState->setMemoryObjectControlState(args.gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST));
    }

    encodeExtraCacheSettings(surfaceState, *args.gmmHelper->getHardwareInfo());

    encodeImplicitScalingParams(args);

    if (EncodeSurfaceState<Family>::isAuxModeEnabled(surfaceState, gmm)) {
        auto resourceFormat = gmm->gmmResourceInfo->getResourceFormat();
        compressionFormat = args.gmmHelper->getClientContext()->getSurfaceStateCompressionFormat(resourceFormat);

        if (DebugManager.flags.ForceBufferCompressionFormat.get() != -1) {
            compressionFormat = DebugManager.flags.ForceBufferCompressionFormat.get();
        }
    }

    if (DebugManager.flags.EnableStatelessCompressionWithUnifiedMemory.get()) {
        if (args.allocation && !MemoryPoolHelper::isSystemMemoryPool(args.allocation->getMemoryPool())) {
            setCoherencyType(surfaceState, R_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT);
            setBufferAuxParamsForCCS(surfaceState);
            compressionFormat = DebugManager.flags.FormatForStatelessCompressionWithUnifiedMemory.get();
        }
    }

    surfaceState->setCompressionFormat(compressionFormat);
}

template <typename Family>
inline void EncodeSurfaceState<Family>::setCoherencyType(R_SURFACE_STATE *surfaceState, COHERENCY_TYPE coherencyType) {
    surfaceState->setCoherencyType(R_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT);
}

template <typename Family>
void EncodeSempahore<Family>::programMiSemaphoreWait(MI_SEMAPHORE_WAIT *cmd,
                                                     uint64_t compareAddress,
                                                     uint32_t compareData,
                                                     COMPARE_OPERATION compareMode,
                                                     bool registerPollMode) {
    MI_SEMAPHORE_WAIT localCmd = Family::cmdInitMiSemaphoreWait;
    localCmd.setCompareOperation(compareMode);
    localCmd.setSemaphoreDataDword(compareData);
    localCmd.setSemaphoreGraphicsAddress(compareAddress);
    localCmd.setWaitMode(MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE);
    localCmd.setRegisterPollMode(registerPollMode ? MI_SEMAPHORE_WAIT::REGISTER_POLL_MODE::REGISTER_POLL_MODE_REGISTER_POLL : MI_SEMAPHORE_WAIT::REGISTER_POLL_MODE::REGISTER_POLL_MODE_MEMORY_POLL);

    *cmd = localCmd;
}

template <typename Family>
inline void EncodeWA<Family>::encodeAdditionalPipelineSelect(LinearStream &stream, const PipelineSelectArgs &args, bool is3DPipeline,
                                                             const HardwareInfo &hwInfo, bool isRcs) {}

template <typename Family>
inline size_t EncodeWA<Family>::getAdditionalPipelineSelectSize(Device &device) {
    return 0u;
}
template <typename Family>
inline void EncodeWA<Family>::addPipeControlPriorToNonPipelinedStateCommand(LinearStream &commandStream, PipeControlArgs args,
                                                                            const HardwareInfo &hwInfo, bool isRcs) {
    auto &hwInfoConfig = (*HwInfoConfig::get(hwInfo.platform.eProductFamily));
    const auto &[isBasicWARequired, isExtendedWARequired] = hwInfoConfig.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

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
void EncodeWA<Family>::adjustCompressionFormatForPlanarImage(uint32_t &compressionFormat, GMM_YUV_PLANE_ENUM plane) {
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
    *cmdBuffer = storeDataImmediate;
}

template <typename Family>
inline void EncodeMiArbCheck<Family>::adjust(MI_ARB_CHECK &miArbCheck) {
    if (DebugManager.flags.ForcePreParserEnabledForMiArbCheck.get() != -1) {
        miArbCheck.setPreParserDisable(!DebugManager.flags.ForcePreParserEnabledForMiArbCheck.get());
    }
}

template <typename Family>
inline void EncodeStoreMMIO<Family>::appendFlags(MI_STORE_REGISTER_MEM *storeRegMem, bool workloadPartition) {
    storeRegMem->setMmioRemapEnable(true);
    storeRegMem->setWorkloadPartitionIdOffsetEnable(workloadPartition);
}

template <typename Family>
void EncodeDispatchKernel<Family>::adjustWalkOrder(WALKER_TYPE &walkerCmd, uint32_t requiredWorkGroupOrder, const HardwareInfo &hwInfo) {}

} // namespace NEO
