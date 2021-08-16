/*
 * Copyright (C) 2020-2021 Intel Corporation
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
#include "shared/source/helpers/pipeline_select_helper.h"
#include "shared/source/helpers/ray_tracing_helper.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/os_interface/hw_info_config.h"

#include "pipe_control_args.h"

#include <algorithm>

namespace NEO {
constexpr size_t TimestampDestinationAddressAlignment = 16;

template <typename Family>
void EncodeDispatchKernel<Family>::encode(CommandContainer &container,
                                          const void *pThreadGroupDimensions, bool isIndirect, bool isPredicate, DispatchKernelEncoderI *dispatchInterface,
                                          uint64_t eventAddress, bool isTimestampEvent, bool L3FlushEnable, Device *device, PreemptionMode preemptionMode,
                                          bool &requiresUncachedMocs, bool useGlobalAtomics, uint32_t &partitionCount, bool isInternal) {
    using SHARED_LOCAL_MEMORY_SIZE = typename Family::INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE;
    using STATE_BASE_ADDRESS = typename Family::STATE_BASE_ADDRESS;
    using MI_BATCH_BUFFER_END = typename Family::MI_BATCH_BUFFER_END;
    using INLINE_DATA = typename Family::INLINE_DATA;

    const HardwareInfo &hwInfo = device->getHardwareInfo();

    const auto &kernelDescriptor = dispatchInterface->getKernelDescriptor();
    auto sizeCrossThreadData = dispatchInterface->getCrossThreadDataSize();
    auto sizePerThreadDataForWholeGroup = dispatchInterface->getPerThreadDataSizeForWholeThreadGroup();

    LinearStream *listCmdBufferStream = container.getCommandStream();
    size_t sshOffset = 0;

    auto threadDims = static_cast<const uint32_t *>(pThreadGroupDimensions);
    const Vec3<size_t> threadStartVec{0, 0, 0};
    Vec3<size_t> threadDimsVec{0, 0, 0};
    if (!isIndirect) {
        threadDimsVec = {threadDims[0], threadDims[1], threadDims[2]};
    }
    size_t estimatedSizeRequired = estimateEncodeDispatchKernelCmdsSize(device, threadStartVec, threadDimsVec, isInternal);
    if (container.getCommandStream()->getAvailableSpace() < estimatedSizeRequired) {
        auto bbEnd = listCmdBufferStream->getSpaceForCmd<MI_BATCH_BUFFER_END>();
        *bbEnd = Family::cmdInitBatchBufferEnd;

        container.allocateNextCommandBuffer();
    }

    if (kernelDescriptor.extendedInfo) {
        bool specialModeRequired = kernelDescriptor.extendedInfo->specialPipelineSelectModeRequired();
        if (container.lastPipelineSelectModeRequired != specialModeRequired) {
            container.lastPipelineSelectModeRequired = specialModeRequired;
            EncodeComputeMode<Family>::adjustPipelineSelect(container, kernelDescriptor);
        }
    }

    WALKER_TYPE walkerCmd = Family::cmdInitGpgpuWalker;
    auto &idd = walkerCmd.getInterfaceDescriptor();

    bool localIdsGenerationByRuntime = dispatchInterface->requiresGenerationOfLocalIdsByRuntime();
    bool inlineDataProgramming = EncodeDispatchKernel<Family>::inlineDataProgrammingRequired(kernelDescriptor);
    {
        auto alloc = dispatchInterface->getIsaAllocation();
        UNRECOVERABLE_IF(nullptr == alloc);
        auto offset = alloc->getGpuAddressToPatch();
        if (!localIdsGenerationByRuntime) {
            offset += kernelDescriptor.entryPoints.skipPerThreadDataLoad;
        }
        idd.setKernelStartPointer(offset);
        idd.setKernelStartPointerHigh(0u);
    }

    auto threadsPerThreadGroup = dispatchInterface->getNumThreadsPerThreadGroup();
    idd.setNumberOfThreadsInGpgpuThreadGroup(threadsPerThreadGroup);

    EncodeDispatchKernel<Family>::programBarrierEnable(idd,
                                                       kernelDescriptor.kernelAttributes.barrierCount,
                                                       hwInfo);

    auto slmSize = static_cast<SHARED_LOCAL_MEMORY_SIZE>(
        HwHelperHw<Family>::get().computeSlmValues(hwInfo, dispatchInterface->getSlmTotalSize()));

    if (DebugManager.flags.OverrideSlmAllocationSize.get() != -1) {
        slmSize = static_cast<SHARED_LOCAL_MEMORY_SIZE>(DebugManager.flags.OverrideSlmAllocationSize.get());
    }
    idd.setSharedLocalMemorySize(slmSize);

    auto bindingTableStateCount = kernelDescriptor.payloadMappings.bindingTable.numEntries;
    uint32_t bindingTablePointer = 0u;
    if (kernelDescriptor.kernelAttributes.bufferAddressingMode == KernelDescriptor::BindfulAndStateless) {
        container.prepareBindfulSsh();
        if (bindingTableStateCount > 0u) {
            auto ssh = container.getHeapWithRequiredSizeAndAlignment(HeapType::SURFACE_STATE, dispatchInterface->getSurfaceStateHeapDataSize(), BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE);
            sshOffset = ssh->getUsed();
            bindingTablePointer = static_cast<uint32_t>(EncodeSurfaceState<Family>::pushBindingTableAndSurfaceStates(
                *ssh, bindingTableStateCount,
                dispatchInterface->getSurfaceStateHeapData(),
                dispatchInterface->getSurfaceStateHeapDataSize(), bindingTableStateCount,
                kernelDescriptor.payloadMappings.bindingTable.tableOffset));
        }
    }
    idd.setBindingTablePointer(bindingTablePointer);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<Family>(&idd, preemptionMode);

    auto heap = ApiSpecificConfig::getBindlessConfiguration() ? device->getBindlessHeapsHelper()->getHeap(BindlessHeapsHelper::GLOBAL_DSH) : container.getIndirectHeap(HeapType::DYNAMIC_STATE);
    UNRECOVERABLE_IF(!heap);

    uint32_t samplerStateOffset = 0;
    uint32_t samplerCount = 0;

    if (kernelDescriptor.payloadMappings.samplerTable.numSamplers > 0) {
        samplerCount = kernelDescriptor.payloadMappings.samplerTable.numSamplers;
        samplerStateOffset = EncodeStates<Family>::copySamplerState(
            heap, kernelDescriptor.payloadMappings.samplerTable.tableOffset,
            kernelDescriptor.payloadMappings.samplerTable.numSamplers, kernelDescriptor.payloadMappings.samplerTable.borderColor,
            dispatchInterface->getDynamicStateHeapData(),
            device->getBindlessHeapsHelper());
        if (ApiSpecificConfig::getBindlessConfiguration()) {
            container.getResidencyContainer().push_back(device->getBindlessHeapsHelper()->getHeap(NEO::BindlessHeapsHelper::BindlesHeapType::GLOBAL_DSH)->getGraphicsAllocation());
        }
    }

    idd.setSamplerStatePointer(samplerStateOffset);

    EncodeDispatchKernel<Family>::adjustBindingTablePrefetch(idd, samplerCount, bindingTableStateCount);

    uint64_t offsetThreadData = 0u;
    const uint32_t inlineDataSize = sizeof(INLINE_DATA);
    auto crossThreadData = dispatchInterface->getCrossThreadData();

    if (inlineDataProgramming) {
        auto copySize = std::min(inlineDataSize, sizeCrossThreadData);
        auto dest = reinterpret_cast<char *>(walkerCmd.getInlineDataPointer());
        memcpy_s(dest, copySize, crossThreadData, copySize);
        auto offset = std::min(inlineDataSize, sizeCrossThreadData);
        sizeCrossThreadData -= copySize;
        crossThreadData = ptrOffset(crossThreadData, offset);
        inlineDataProgramming = copySize != 0;
    }

    uint32_t sizeThreadData = sizePerThreadDataForWholeGroup + sizeCrossThreadData;
    {
        auto heap = container.getIndirectHeap(HeapType::INDIRECT_OBJECT);
        UNRECOVERABLE_IF(!heap);
        heap->align(WALKER_TYPE::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);

        auto ptr = container.getHeapSpaceAllowGrow(HeapType::INDIRECT_OBJECT, sizeThreadData);
        UNRECOVERABLE_IF(!ptr);
        offsetThreadData = (is64bit ? heap->getHeapGpuStartOffset() : heap->getHeapGpuBase()) + static_cast<uint64_t>(heap->getUsed() - sizeThreadData);

        if (sizeCrossThreadData > 0) {
            memcpy_s(ptr, sizeCrossThreadData,
                     crossThreadData, sizeCrossThreadData);
        }
        if (isIndirect) {
            void *gpuPtr = reinterpret_cast<void *>(heap->getHeapGpuBase() + heap->getUsed() - sizeThreadData);
            EncodeIndirectParams<Family>::setGroupCountIndirect(container, kernelDescriptor.payloadMappings.dispatchTraits.numWorkGroups, gpuPtr);
            EncodeIndirectParams<Family>::setGlobalWorkSizeIndirect(container, kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize, gpuPtr, dispatchInterface->getGroupSize());
            EncodeIndirectParams<Family>::setWorkDimIndirect(container, kernelDescriptor.payloadMappings.dispatchTraits.workDim, gpuPtr, dispatchInterface->getGroupSize());
        }

        auto perThreadDataPtr = dispatchInterface->getPerThreadData();
        if (perThreadDataPtr != nullptr) {
            ptr = ptrOffset(ptr, sizeCrossThreadData);
            memcpy_s(ptr, sizePerThreadDataForWholeGroup,
                     perThreadDataPtr, sizePerThreadDataForWholeGroup);
        }
    }

    bool requiresGlobalAtomicsUpdate = false;
    if (ImplicitScalingHelper::isImplicitScalingEnabled(container.getDevice()->getDeviceBitfield(), true)) {
        requiresGlobalAtomicsUpdate = container.lastSentUseGlobalAtomics != useGlobalAtomics;
        container.lastSentUseGlobalAtomics = useGlobalAtomics;
    }

    if (container.isAnyHeapDirty() || requiresUncachedMocs || requiresGlobalAtomicsUpdate) {
        PipeControlArgs args(true);
        MemorySynchronizationCommands<Family>::addPipeControl(*container.getCommandStream(), args);
        STATE_BASE_ADDRESS sbaCmd;
        auto gmmHelper = container.getDevice()->getGmmHelper();
        uint32_t statelessMocsIndex =
            requiresUncachedMocs ? (gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) >> 1) : (gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) >> 1);
        EncodeStateBaseAddress<Family>::encode(container, sbaCmd, statelessMocsIndex, useGlobalAtomics);
        container.setDirtyStateForAllHeaps(false);
        requiresUncachedMocs = false;
    }

    walkerCmd.setIndirectDataStartAddress(static_cast<uint32_t>(offsetThreadData));
    walkerCmd.setIndirectDataLength(sizeThreadData);

    EncodeDispatchKernel<Family>::encodeThreadData(walkerCmd,
                                                   nullptr,
                                                   threadDims,
                                                   dispatchInterface->getGroupSize(),
                                                   kernelDescriptor.kernelAttributes.simdSize,
                                                   kernelDescriptor.kernelAttributes.numLocalIdChannels,
                                                   dispatchInterface->getNumThreadsPerThreadGroup(),
                                                   dispatchInterface->getThreadExecutionMask(),
                                                   localIdsGenerationByRuntime,
                                                   inlineDataProgramming,
                                                   isIndirect,
                                                   dispatchInterface->getRequiredWorkgroupOrder());

    using POSTSYNC_DATA = typename Family::POSTSYNC_DATA;
    auto &postSync = walkerCmd.getPostSync();
    if (eventAddress != 0) {
        postSync.setDataportPipelineFlush(true);
        postSync.setL3Flush(L3FlushEnable);
        if (isTimestampEvent) {
            postSync.setOperation(POSTSYNC_DATA::OPERATION_WRITE_TIMESTAMP);
        } else {
            uint32_t STATE_SIGNALED = 0u;
            postSync.setOperation(POSTSYNC_DATA::OPERATION_WRITE_IMMEDIATE_DATA);
            postSync.setImmediateData(STATE_SIGNALED);
        }
        UNRECOVERABLE_IF(!(isAligned<TimestampDestinationAddressAlignment>(eventAddress)));
        postSync.setDestinationAddress(eventAddress);

        auto gmmHelper = device->getRootDeviceEnvironment().getGmmHelper();
        postSync.setMocs(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED));

        EncodeDispatchKernel<Family>::adjustTimestampPacket(walkerCmd, hwInfo);
    }

    walkerCmd.setPredicateEnable(isPredicate);

    EncodeDispatchKernel<Family>::adjustInterfaceDescriptorData(idd, hwInfo);

    EncodeDispatchKernel<Family>::appendAdditionalIDDFields(&idd, hwInfo, threadsPerThreadGroup,
                                                            dispatchInterface->getSlmTotalSize(),
                                                            dispatchInterface->getSlmPolicy());

    EncodeDispatchKernel<Family>::encodeAdditionalWalkerFields(hwInfo, walkerCmd);

    PreemptionHelper::applyPreemptionWaCmdsBegin<Family>(listCmdBufferStream, *device);

    if (ImplicitScalingHelper::isImplicitScalingEnabled(device->getDeviceBitfield(), true) &&
        !isInternal) {
        const uint64_t workPartitionAllocationGpuVa = device->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocationGpuAddress();
        ImplicitScalingDispatch<Family>::dispatchCommands(*listCmdBufferStream,
                                                          walkerCmd,
                                                          device->getDeviceBitfield(),
                                                          partitionCount,
                                                          true,
                                                          true,
                                                          false,
                                                          workPartitionAllocationGpuVa);
    } else {
        partitionCount = 1;
        auto buffer = listCmdBufferStream->getSpace(sizeof(walkerCmd));
        *(decltype(walkerCmd) *)buffer = walkerCmd;
    }

    PreemptionHelper::applyPreemptionWaCmdsEnd<Family>(listCmdBufferStream, *device);
}

template <typename Family>
inline void EncodeDispatchKernel<Family>::encodeAdditionalWalkerFields(const HardwareInfo &hwInfo, WALKER_TYPE &walkerCmd) {
}

template <typename Family>
bool EncodeDispatchKernel<Family>::isRuntimeLocalIdsGenerationRequired(uint32_t activeChannels,
                                                                       size_t *lws,
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

        //make sure table below matches Hardware Spec
        constexpr uint32_t walkOrderPossibilties = 6u;
        constexpr uint8_t possibleWalkOrders[walkOrderPossibilties][3] = {{0, 1, 2},
                                                                          {0, 2, 1},
                                                                          {1, 0, 2},
                                                                          {2, 0, 1},
                                                                          {1, 2, 0},
                                                                          {2, 1, 0}};

        //check if we need to follow kernel requirements
        if (requireInputWalkOrder) {
            for (uint32_t dimension = 0; dimension < activeChannels - 1; dimension++) {
                if (!Math::isPow2<size_t>(lws[walkOrder[dimension]])) {
                    return true;
                }
            }

            auto index = 0u;
            while (index < walkOrderPossibilties) {
                if (walkOrder[0] == possibleWalkOrders[index][0] &&
                    walkOrder[1] == possibleWalkOrders[index][1]) {
                    break;
                };
                index++;
            }
            DEBUG_BREAK_IF(index >= walkOrderPossibilties);

            requiredWalkOrder = index;
            return false;
        }

        //kernel doesn't specify any walk order requirements, check if we have any compatible
        for (uint32_t walkOrder = 0; walkOrder < walkOrderPossibilties; walkOrder++) {
            bool allDimensionsCompatible = true;
            for (uint32_t dimension = 0; dimension < activeChannels - 1; dimension++) {
                if (!Math::isPow2<size_t>(lws[possibleWalkOrders[walkOrder][dimension]])) {
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
                                                    uint32_t requiredWorkGroupOrder) {

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

    //1) cross-thread inline data will be put into R1, but if kernel uses local ids, then cross-thread should be put further back
    //so whenever local ids are driver or hw generated, reserve space by setting right values for emitLocalIds
    //2) Auto-generation of local ids should be possible, when in fact local ids are used
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
    if (inlineDataProgrammingRequired == true) {
        walkerCmd.setEmitInlineParameter(1);
    }
}

template <typename Family>
size_t EncodeDispatchKernel<Family>::estimateEncodeDispatchKernelCmdsSize(Device *device, Vec3<size_t> groupStart, Vec3<size_t> groupCount,
                                                                          bool isInternal) {
    size_t totalSize = sizeof(WALKER_TYPE);
    totalSize += PreemptionHelper::getPreemptionWaCsSize<Family>(*device);
    totalSize += EncodeStates<Family>::getAdjustStateComputeModeSize();
    totalSize += EncodeIndirectParams<Family>::getCmdsSizeForIndirectParams();
    totalSize += EncodeIndirectParams<Family>::getCmdsSizeForSetGroupCountIndirect();
    totalSize += EncodeIndirectParams<Family>::getCmdsSizeForSetGroupSizeIndirect();
    if (ImplicitScalingHelper::isImplicitScalingEnabled(device->getDeviceBitfield(), true) &&
        !isInternal) {
        const bool staticPartitioning = device->getDefaultEngine().commandStreamReceiver->isStaticWorkPartitioningEnabled();
        totalSize += ImplicitScalingDispatch<Family>::getSize(true, staticPartitioning, device->getDeviceBitfield(), groupStart, groupCount);
    }

    return totalSize;
}

template <typename Family>
void EncodeStateBaseAddress<Family>::encode(CommandContainer &container, STATE_BASE_ADDRESS &sbaCmd) {
    auto gmmHelper = container.getDevice()->getRootDeviceEnvironment().getGmmHelper();
    uint32_t statelessMocsIndex = (gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) >> 1);
    EncodeStateBaseAddress<Family>::encode(container, sbaCmd, statelessMocsIndex, false);
}

template <typename Family>
void EncodeStateBaseAddress<Family>::encode(CommandContainer &container, STATE_BASE_ADDRESS &sbaCmd, uint32_t statelessMocsIndex, bool useGlobalAtomics) {
    auto gmmHelper = container.getDevice()->getRootDeviceEnvironment().getGmmHelper();
    bool multiOsContextCapable =
        ImplicitScalingHelper::isImplicitScalingEnabled(container.getDevice()->getDeviceBitfield(), true);

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
        1u);

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
void EncodeComputeMode<Family>::adjustComputeMode(LinearStream &csr, void *const stateComputeModePtr, StateComputeModeProperties &properties, const HardwareInfo &hwInfo) {
    using STATE_COMPUTE_MODE = typename Family::STATE_COMPUTE_MODE;
    using FORCE_NON_COHERENT = typename STATE_COMPUTE_MODE::FORCE_NON_COHERENT;

    STATE_COMPUTE_MODE stateComputeMode = (stateComputeModePtr != nullptr) ? *(static_cast<STATE_COMPUTE_MODE *>(stateComputeModePtr)) : Family::cmdInitStateComputeMode;
    auto maskBits = stateComputeMode.getMaskBits();

    if (properties.isCoherencyRequired.isDirty) {
        FORCE_NON_COHERENT coherencyValue = !properties.isCoherencyRequired.value ? FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT
                                                                                  : FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_DISABLED;
        stateComputeMode.setForceNonCoherent(coherencyValue);
        maskBits |= Family::stateComputeModeForceNonCoherentMask;
    }

    if (properties.largeGrfMode.isDirty) {
        stateComputeMode.setLargeGrfMode(properties.largeGrfMode.value);
        maskBits |= Family::stateComputeModeLargeGrfModeMask;
    }

    stateComputeMode.setMaskBits(maskBits);

    auto buffer = csr.getSpaceForCmd<STATE_COMPUTE_MODE>();
    *buffer = stateComputeMode;
}

template <typename Family>
void EncodeComputeMode<Family>::adjustPipelineSelect(CommandContainer &container, const NEO::KernelDescriptor &kernelDescriptor) {
    using PIPELINE_SELECT = typename Family::PIPELINE_SELECT;
    auto pipelineSelectCmd = Family::cmdInitPipelineSelect;

    if (kernelDescriptor.extendedInfo && kernelDescriptor.extendedInfo->specialPipelineSelectModeRequired()) {
        pipelineSelectCmd.setSystolicModeEnable(true);
    } else {
        pipelineSelectCmd.setSystolicModeEnable(false);
    }

    if (DebugManager.flags.OverrideSystolicPipelineSelect.get() != -1) {
        pipelineSelectCmd.setSystolicModeEnable(DebugManager.flags.OverrideSystolicPipelineSelect.get());
    }

    pipelineSelectCmd.setMaskBits(pipelineSelectSystolicModeEnableMaskBits);
    pipelineSelectCmd.setPipelineSelection(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);

    auto buffer = container.getCommandStream()->getSpace(sizeof(pipelineSelectCmd));
    *(decltype(pipelineSelectCmd) *)buffer = pipelineSelectCmd;
}

template <typename Family>
inline void EncodeMediaInterfaceDescriptorLoad<Family>::encode(CommandContainer &container) {
}

template <typename Family>
void EncodeMiFlushDW<Family>::appendMiFlushDw(MI_FLUSH_DW *miFlushDwCmd) {
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

template <typename GfxFamily>
void EncodeSurfaceState<GfxFamily>::encodeExtraBufferParams(R_SURFACE_STATE *surfaceState, GraphicsAllocation *allocation, GmmHelper *gmmHelper,
                                                            bool isReadOnly, uint32_t numAvailableDevices, bool useGlobalAtomics, bool areMultipleSubDevicesInContext) {
    Gmm *gmm = allocation ? allocation->getDefaultGmm() : nullptr;
    uint32_t compressionFormat = 0;

    bool setConstCachePolicy = false;
    if (allocation && allocation->getAllocationType() == GraphicsAllocation::AllocationType::CONSTANT_SURFACE) {
        setConstCachePolicy = true;
    }

    if (surfaceState->getMemoryObjectControlState() == gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) &&
        DebugManager.flags.ForceL1Caching.get() != 0) {
        setConstCachePolicy = true;
    }

    if (setConstCachePolicy == true) {
        surfaceState->setMemoryObjectControlState(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST));
    }

    encodeExtraCacheSettings(surfaceState, *gmmHelper->getHardwareInfo());
    DeviceBitfield deviceBitfield{static_cast<uint32_t>(maxNBitValue(numAvailableDevices))};
    bool implicitScaling = ImplicitScalingHelper::isImplicitScalingEnabled(deviceBitfield, true);
    bool enablePartialWrites = implicitScaling;
    bool enableMultiGpuAtomics = enablePartialWrites;

    if (DebugManager.flags.EnableMultiGpuAtomicsOptimization.get()) {
        enableMultiGpuAtomics = useGlobalAtomics && (enablePartialWrites || areMultipleSubDevicesInContext);
    }

    surfaceState->setDisableSupportForMultiGpuAtomics(!enableMultiGpuAtomics);
    surfaceState->setDisableSupportForMultiGpuPartialWrites(!enablePartialWrites);

    if (DebugManager.flags.ForceMultiGpuAtomics.get() != -1) {
        surfaceState->setDisableSupportForMultiGpuAtomics(!!DebugManager.flags.ForceMultiGpuAtomics.get());
    }

    if (DebugManager.flags.ForceMultiGpuPartialWrites.get() != -1) {
        surfaceState->setDisableSupportForMultiGpuPartialWrites(!!DebugManager.flags.ForceMultiGpuPartialWrites.get());
    }

    if (EncodeSurfaceState<GfxFamily>::isAuxModeEnabled(surfaceState, gmm)) {
        auto resourceFormat = gmm->gmmResourceInfo->getResourceFormat();
        compressionFormat = gmmHelper->getClientContext()->getSurfaceStateCompressionFormat(resourceFormat);

        if (DebugManager.flags.ForceBufferCompressionFormat.get() != -1) {
            compressionFormat = DebugManager.flags.ForceBufferCompressionFormat.get();
        }
    }

    if (DebugManager.flags.EnableStatelessCompressionWithUnifiedMemory.get()) {
        if (allocation && !MemoryPool::isSystemMemoryPool(allocation->getMemoryPool())) {
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
inline void EncodeWA<Family>::encodeAdditionalPipelineSelect(Device &device, LinearStream &stream, bool is3DPipeline) {}

template <typename Family>
inline size_t EncodeWA<Family>::getAdditionalPipelineSelectSize(Device &device) {
    return 0u;
}
} // namespace NEO
