/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"
#include "shared/source/kernel/implicit_args.h"

#include "pipe_control_args.h"

#include <algorithm>

namespace NEO {
template <typename Family>
void EncodeDispatchKernel<Family>::encode(CommandContainer &container,
                                          const void *pThreadGroupDimensions, bool isIndirect, bool isPredicate, DispatchKernelEncoderI *dispatchInterface,
                                          uint64_t eventAddress, bool isTimestampEvent, bool L3FlushEnable, Device *device, PreemptionMode preemptionMode,
                                          bool &requiresUncachedMocs, bool useGlobalAtomics, uint32_t &partitionCount, bool isInternal, bool isCooperative) {

    using MEDIA_STATE_FLUSH = typename Family::MEDIA_STATE_FLUSH;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename Family::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    using MI_BATCH_BUFFER_END = typename Family::MI_BATCH_BUFFER_END;
    using STATE_BASE_ADDRESS = typename Family::STATE_BASE_ADDRESS;

    auto &kernelDescriptor = dispatchInterface->getKernelDescriptor();
    auto sizeCrossThreadData = dispatchInterface->getCrossThreadDataSize();
    auto sizePerThreadData = dispatchInterface->getPerThreadDataSize();
    auto sizePerThreadDataForWholeGroup = dispatchInterface->getPerThreadDataSizeForWholeThreadGroup();
    auto pImplicitArgs = dispatchInterface->getImplicitArgs();

    const HardwareInfo &hwInfo = device->getHardwareInfo();

    LinearStream *listCmdBufferStream = container.getCommandStream();
    size_t sshOffset = 0;

    auto threadDims = static_cast<const uint32_t *>(pThreadGroupDimensions);
    const Vec3<size_t> threadStartVec{0, 0, 0};
    Vec3<size_t> threadDimsVec{0, 0, 0};
    if (!isIndirect) {
        threadDimsVec = {threadDims[0], threadDims[1], threadDims[2]};
    }
    size_t estimatedSizeRequired = estimateEncodeDispatchKernelCmdsSize(device, threadStartVec, threadDimsVec,
                                                                        isInternal, isCooperative, isIndirect, dispatchInterface);
    if (container.getCommandStream()->getAvailableSpace() < estimatedSizeRequired) {
        auto bbEnd = listCmdBufferStream->getSpaceForCmd<MI_BATCH_BUFFER_END>();
        *bbEnd = Family::cmdInitBatchBufferEnd;

        container.allocateNextCommandBuffer();
    }

    WALKER_TYPE cmd = Family::cmdInitGpgpuWalker;
    auto idd = Family::cmdInitInterfaceDescriptorData;

    {
        auto alloc = dispatchInterface->getIsaAllocation();
        UNRECOVERABLE_IF(nullptr == alloc);
        auto offset = alloc->getGpuAddressToPatch();
        idd.setKernelStartPointer(offset);
        idd.setKernelStartPointerHigh(0u);
    }

    auto numThreadsPerThreadGroup = dispatchInterface->getNumThreadsPerThreadGroup();
    idd.setNumberOfThreadsInGpgpuThreadGroup(numThreadsPerThreadGroup);

    EncodeDispatchKernel<Family>::programBarrierEnable(idd,
                                                       kernelDescriptor.kernelAttributes.barrierCount,
                                                       hwInfo);
    auto slmSize = static_cast<typename INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE>(
        HwHelperHw<Family>::get().computeSlmValues(hwInfo, dispatchInterface->getSlmTotalSize()));
    idd.setSharedLocalMemorySize(slmSize);

    uint32_t bindingTableStateCount = kernelDescriptor.payloadMappings.bindingTable.numEntries;
    uint32_t bindingTablePointer = 0u;
    bool isBindlessKernel = kernelDescriptor.kernelAttributes.bufferAddressingMode == KernelDescriptor::BindlessAndStateless;
    if (!isBindlessKernel) {
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
        samplerStateOffset = EncodeStates<Family>::copySamplerState(heap, kernelDescriptor.payloadMappings.samplerTable.tableOffset,
                                                                    kernelDescriptor.payloadMappings.samplerTable.numSamplers,
                                                                    kernelDescriptor.payloadMappings.samplerTable.borderColor,
                                                                    dispatchInterface->getDynamicStateHeapData(),
                                                                    device->getBindlessHeapsHelper(), device->getHardwareInfo());
        if (ApiSpecificConfig::getBindlessConfiguration()) {
            container.getResidencyContainer().push_back(device->getBindlessHeapsHelper()->getHeap(NEO::BindlessHeapsHelper::BindlesHeapType::GLOBAL_DSH)->getGraphicsAllocation());
        }
    }

    idd.setSamplerStatePointer(samplerStateOffset);
    if (!isBindlessKernel) {
        EncodeDispatchKernel<Family>::adjustBindingTablePrefetch(idd, samplerCount, bindingTableStateCount);
    }

    auto numGrfCrossThreadData = static_cast<uint32_t>(sizeCrossThreadData / sizeof(float[8]));
    idd.setCrossThreadConstantDataReadLength(numGrfCrossThreadData);

    auto numGrfPerThreadData = static_cast<uint32_t>(sizePerThreadData / sizeof(float[8]));
    DEBUG_BREAK_IF(numGrfPerThreadData <= 0u);
    idd.setConstantIndirectUrbEntryReadLength(numGrfPerThreadData);

    uint32_t sizeThreadData = sizePerThreadDataForWholeGroup + sizeCrossThreadData;
    uint32_t sizeForImplicitArgsPatching = dispatchInterface->getSizeForImplicitArgsPatching();
    uint32_t iohRequiredSize = sizeThreadData + sizeForImplicitArgsPatching;
    uint64_t offsetThreadData = 0u;
    {
        auto heapIndirect = container.getIndirectHeap(HeapType::INDIRECT_OBJECT);
        UNRECOVERABLE_IF(!(heapIndirect));
        heapIndirect->align(WALKER_TYPE::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);

        auto ptr = container.getHeapSpaceAllowGrow(HeapType::INDIRECT_OBJECT, iohRequiredSize);
        UNRECOVERABLE_IF(!(ptr));
        offsetThreadData = heapIndirect->getHeapGpuStartOffset() + static_cast<uint64_t>(heapIndirect->getUsed() - sizeThreadData);

        if (pImplicitArgs) {
            offsetThreadData -= sizeof(ImplicitArgs);
            pImplicitArgs->localIdTablePtr = heapIndirect->getGraphicsAllocation()->getGpuAddress() + heapIndirect->getUsed() - iohRequiredSize;
            dispatchInterface->patchImplicitArgs(ptr);
        }

        memcpy_s(ptr, sizeCrossThreadData,
                 dispatchInterface->getCrossThreadData(), sizeCrossThreadData);

        if (isIndirect) {
            auto gpuPtr = heapIndirect->getGraphicsAllocation()->getGpuAddress() + heapIndirect->getUsed() - sizeThreadData;
            uint64_t implicitArgsGpuPtr = 0u;
            if (pImplicitArgs) {
                implicitArgsGpuPtr = gpuPtr - sizeof(ImplicitArgs);
            }
            EncodeIndirectParams<Family>::encode(container, gpuPtr, dispatchInterface, implicitArgsGpuPtr);
        }

        ptr = ptrOffset(ptr, sizeCrossThreadData);
        memcpy_s(ptr, sizePerThreadDataForWholeGroup,
                 dispatchInterface->getPerThreadData(), sizePerThreadDataForWholeGroup);
    }

    auto slmSizeNew = dispatchInterface->getSlmTotalSize();
    bool dirtyHeaps = container.isAnyHeapDirty();
    bool flush = container.slmSize != slmSizeNew || dirtyHeaps || requiresUncachedMocs;

    if (flush) {
        PipeControlArgs args(true);
        if (dirtyHeaps) {
            args.hdcPipelineFlush = true;
        }
        MemorySynchronizationCommands<Family>::addPipeControl(*container.getCommandStream(), args);

        if (dirtyHeaps || requiresUncachedMocs) {
            STATE_BASE_ADDRESS sba;
            auto gmmHelper = container.getDevice()->getGmmHelper();
            uint32_t statelessMocsIndex =
                requiresUncachedMocs ? (gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) >> 1) : (gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) >> 1);
            EncodeStateBaseAddress<Family>::encode(container, sba, statelessMocsIndex, false);
            container.setDirtyStateForAllHeaps(false);
            requiresUncachedMocs = false;
        }

        if (container.slmSize != slmSizeNew) {
            EncodeL3State<Family>::encode(container, slmSizeNew != 0u);
            container.slmSize = slmSizeNew;

            if (container.nextIddInBlock != container.getNumIddPerBlock()) {
                EncodeMediaInterfaceDescriptorLoad<Family>::encode(container);
            }
        }
    }

    uint32_t numIDD = 0u;
    void *ptr = getInterfaceDescriptor(container, numIDD);
    memcpy_s(ptr, sizeof(idd), &idd, sizeof(idd));

    cmd.setIndirectDataStartAddress(static_cast<uint32_t>(offsetThreadData));
    cmd.setIndirectDataLength(sizeThreadData);
    cmd.setInterfaceDescriptorOffset(numIDD);

    EncodeDispatchKernel<Family>::encodeThreadData(cmd,
                                                   nullptr,
                                                   threadDims,
                                                   dispatchInterface->getGroupSize(),
                                                   kernelDescriptor.kernelAttributes.simdSize,
                                                   kernelDescriptor.kernelAttributes.numLocalIdChannels,
                                                   dispatchInterface->getNumThreadsPerThreadGroup(),
                                                   dispatchInterface->getThreadExecutionMask(),
                                                   true,
                                                   false,
                                                   isIndirect,
                                                   dispatchInterface->getRequiredWorkgroupOrder());

    cmd.setPredicateEnable(isPredicate);

    EncodeDispatchKernel<Family>::adjustInterfaceDescriptorData(idd, hwInfo);

    PreemptionHelper::applyPreemptionWaCmdsBegin<Family>(listCmdBufferStream, *device);

    auto buffer = listCmdBufferStream->getSpace(sizeof(cmd));
    *(decltype(cmd) *)buffer = cmd;

    PreemptionHelper::applyPreemptionWaCmdsEnd<Family>(listCmdBufferStream, *device);
    {
        auto mediaStateFlush = listCmdBufferStream->getSpace(sizeof(MEDIA_STATE_FLUSH));
        *reinterpret_cast<MEDIA_STATE_FLUSH *>(mediaStateFlush) = Family::cmdInitMediaStateFlush;
    }

    partitionCount = 1;
}

template <typename Family>
void EncodeMediaInterfaceDescriptorLoad<Family>::encode(CommandContainer &container) {
    using MEDIA_STATE_FLUSH = typename Family::MEDIA_STATE_FLUSH;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename Family::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    auto heapBase = ApiSpecificConfig::getBindlessConfiguration() ? container.getDevice()->getBindlessHeapsHelper()->getHeap(BindlessHeapsHelper::GLOBAL_DSH)->getGraphicsAllocation()->getUnderlyingBuffer() : container.getIndirectHeap(HeapType::DYNAMIC_STATE)->getCpuBase();

    auto mediaStateFlush = container.getCommandStream()->getSpaceForCmd<MEDIA_STATE_FLUSH>();
    *mediaStateFlush = Family::cmdInitMediaStateFlush;

    auto iddOffset = static_cast<uint32_t>(ptrDiff(container.getIddBlock(), heapBase));
    iddOffset += ApiSpecificConfig::getBindlessConfiguration() ? static_cast<uint32_t>(container.getDevice()->getBindlessHeapsHelper()->getHeap(BindlessHeapsHelper::GLOBAL_DSH)->getGraphicsAllocation()->getGpuAddress() -
                                                                                       container.getDevice()->getBindlessHeapsHelper()->getHeap(BindlessHeapsHelper::GLOBAL_DSH)->getGraphicsAllocation()->getGpuBaseAddress())
                                                               : 0;

    MEDIA_INTERFACE_DESCRIPTOR_LOAD cmd = Family::cmdInitMediaInterfaceDescriptorLoad;
    cmd.setInterfaceDescriptorDataStartAddress(iddOffset);
    cmd.setInterfaceDescriptorTotalLength(sizeof(INTERFACE_DESCRIPTOR_DATA) * container.getNumIddPerBlock());

    auto buffer = container.getCommandStream()->getSpace(sizeof(cmd));
    *(decltype(cmd) *)buffer = cmd;
}

template <typename Family>
inline bool EncodeDispatchKernel<Family>::isRuntimeLocalIdsGenerationRequired(uint32_t activeChannels,
                                                                              size_t *lws,
                                                                              std::array<uint8_t, 3> walkOrder,
                                                                              bool requireInputWalkOrder,
                                                                              uint32_t &requiredWalkOrder,
                                                                              uint32_t simd) {
    requiredWalkOrder = 0u;
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
        walkerCmd.setThreadGroupIdStartingResumeZ(static_cast<uint32_t>(startWorkGroup[2]));
    }

    walkerCmd.setSimdSize(getSimdConfig<WALKER_TYPE>(simd));

    auto localWorkSize = workGroupSizes[0] * workGroupSizes[1] * workGroupSizes[2];
    if (threadsPerThreadGroup == 0) {
        threadsPerThreadGroup = static_cast<uint32_t>(getThreadsPerWG(simd, localWorkSize));
    }
    walkerCmd.setThreadWidthCounterMaximum(threadsPerThreadGroup);

    uint64_t executionMask = threadExecutionMask;
    if (executionMask == 0) {
        auto remainderSimdLanes = localWorkSize & (simd - 1);
        executionMask = maxNBitValue(remainderSimdLanes);
        if (!executionMask)
            executionMask = ~executionMask;
    }

    constexpr uint32_t maxDword = std::numeric_limits<uint32_t>::max();
    walkerCmd.setRightExecutionMask(static_cast<uint32_t>(executionMask));
    walkerCmd.setBottomExecutionMask(maxDword);
}

template <typename Family>
void EncodeDispatchKernel<Family>::programBarrierEnable(INTERFACE_DESCRIPTOR_DATA &interfaceDescriptor,
                                                        uint32_t value,
                                                        const HardwareInfo &hwInfo) {
    interfaceDescriptor.setBarrierEnable(value);
}

template <typename Family>
inline void EncodeDispatchKernel<Family>::encodeAdditionalWalkerFields(const HardwareInfo &hwInfo, WALKER_TYPE &walkerCmd) {}

template <typename Family>
void EncodeDispatchKernel<Family>::appendAdditionalIDDFields(INTERFACE_DESCRIPTOR_DATA *pInterfaceDescriptor, const HardwareInfo &hwInfo, const uint32_t threadsPerThreadGroup, uint32_t slmTotalSize, SlmPolicy slmPolicy) {}

template <typename Family>
size_t EncodeDispatchKernel<Family>::estimateEncodeDispatchKernelCmdsSize(Device *device, const Vec3<size_t> &groupStart,
                                                                          const Vec3<size_t> &groupCount, bool isInternal,
                                                                          bool isCooperative, bool isIndirect, DispatchKernelEncoderI *dispatchInterface) {
    using MEDIA_STATE_FLUSH = typename Family::MEDIA_STATE_FLUSH;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename Family::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    using MI_BATCH_BUFFER_END = typename Family::MI_BATCH_BUFFER_END;

    size_t issueMediaInterfaceDescriptorLoad = sizeof(MEDIA_STATE_FLUSH) + sizeof(MEDIA_INTERFACE_DESCRIPTOR_LOAD);
    size_t totalSize = sizeof(WALKER_TYPE);
    totalSize += PreemptionHelper::getPreemptionWaCsSize<Family>(*device);
    totalSize += sizeof(MEDIA_STATE_FLUSH);
    totalSize += issueMediaInterfaceDescriptorLoad;
    totalSize += EncodeStates<Family>::getAdjustStateComputeModeSize();
    totalSize += EncodeWA<Family>::getAdditionalPipelineSelectSize(*device);
    totalSize += EncodeIndirectParams<Family>::getCmdsSizeForIndirectParams();
    totalSize += EncodeIndirectParams<Family>::getCmdsSizeForSetGroupCountIndirect();
    totalSize += EncodeIndirectParams<Family>::getCmdsSizeForSetGroupSizeIndirect();
    if (isIndirect) {
        UNRECOVERABLE_IF(dispatchInterface == nullptr);
        totalSize += EncodeIndirectParams<Family>::getCmdsSizeForSetWorkDimIndirect(dispatchInterface->getGroupSize(), false);
        if (dispatchInterface->getImplicitArgs()) {
            totalSize += EncodeIndirectParams<Family>::getCmdsSizeForSetGroupCountIndirect();
            totalSize += EncodeIndirectParams<Family>::getCmdsSizeForSetGroupSizeIndirect();
            totalSize += EncodeIndirectParams<Family>::getCmdsSizeForSetWorkDimIndirect(dispatchInterface->getGroupSize(), true);
        }
    }

    totalSize += sizeof(MI_BATCH_BUFFER_END);

    return totalSize;
}

template <typename Family>
inline void EncodeComputeMode<Family>::adjustComputeMode(LinearStream &csr, void *const stateComputeModePtr, StateComputeModeProperties &properties, const HardwareInfo &hwInfo) {
}

template <typename Family>
inline void EncodeComputeMode<Family>::adjustPipelineSelect(CommandContainer &container, const NEO::KernelDescriptor &kernelDescriptor) {
}

template <typename Family>
void EncodeStateBaseAddress<Family>::setIohAddressForDebugger(NEO::Debugger::SbaAddresses &sbaAddress, const STATE_BASE_ADDRESS &sbaCmd) {
    sbaAddress.IndirectObjectBaseAddress = sbaCmd.getIndirectObjectBaseAddress();
}

template <typename Family>
void EncodeStateBaseAddress<Family>::encode(CommandContainer &container, STATE_BASE_ADDRESS &sbaCmd) {
    auto gmmHelper = container.getDevice()->getRootDeviceEnvironment().getGmmHelper();
    uint32_t statelessMocsIndex = (gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) >> 1);
    EncodeStateBaseAddress<Family>::encode(container, sbaCmd, statelessMocsIndex, false);
}

template <typename Family>
void EncodeStateBaseAddress<Family>::encode(CommandContainer &container, STATE_BASE_ADDRESS &sbaCmd, uint32_t statelessMocsIndex, bool useGlobalAtomics) {
    if (container.isAnyHeapDirty()) {
        EncodeWA<Family>::encodeAdditionalPipelineSelect(*container.getDevice(), *container.getCommandStream(), true);
    }

    auto gmmHelper = container.getDevice()->getGmmHelper();

    StateBaseAddressHelper<Family>::programStateBaseAddress(
        &sbaCmd,
        container.isHeapDirty(HeapType::DYNAMIC_STATE) ? container.getIndirectHeap(HeapType::DYNAMIC_STATE) : nullptr,
        container.isHeapDirty(HeapType::INDIRECT_OBJECT) ? container.getIndirectHeap(HeapType::INDIRECT_OBJECT) : nullptr,
        container.isHeapDirty(HeapType::SURFACE_STATE) ? container.getIndirectHeap(HeapType::SURFACE_STATE) : nullptr,
        0,
        false,
        statelessMocsIndex,
        container.getIndirectObjectHeapBaseAddress(),
        container.getInstructionHeapBaseAddress(),
        0,
        false,
        false,
        gmmHelper,
        false,
        MemoryCompressionState::NotApplicable,
        useGlobalAtomics,
        1u);

    auto pCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(container.getCommandStream()->getSpace(sizeof(STATE_BASE_ADDRESS)));
    *pCmd = sbaCmd;

    EncodeWA<Family>::encodeAdditionalPipelineSelect(*container.getDevice(), *container.getCommandStream(), false);
}

template <typename Family>
size_t EncodeStateBaseAddress<Family>::getRequiredSizeForStateBaseAddress(Device &device, CommandContainer &container) {
    return sizeof(typename Family::STATE_BASE_ADDRESS) + 2 * EncodeWA<Family>::getAdditionalPipelineSelectSize(device);
}

template <typename Family>
void EncodeL3State<Family>::encode(CommandContainer &container, bool enableSLM) {
    auto offset = L3CNTLRegisterOffset<Family>::registerOffset;
    auto data = PreambleHelper<Family>::getL3Config(container.getDevice()->getHardwareInfo(), enableSLM);
    EncodeSetMMIO<Family>::encodeIMM(container, offset, data, false);
}

template <typename GfxFamily>
void EncodeMiFlushDW<GfxFamily>::appendMiFlushDw(MI_FLUSH_DW *miFlushDwCmd) {}

template <typename GfxFamily>
void EncodeMiFlushDW<GfxFamily>::programMiFlushDwWA(LinearStream &commandStream) {}

template <typename GfxFamily>
size_t EncodeMiFlushDW<GfxFamily>::getMiFlushDwWaSize() {
    return 0;
}

template <typename GfxFamily>
inline void EncodeWA<GfxFamily>::encodeAdditionalPipelineSelect(Device &device, LinearStream &stream, bool is3DPipeline) {}

template <typename GfxFamily>
inline size_t EncodeWA<GfxFamily>::getAdditionalPipelineSelectSize(Device &device) {
    return 0;
}

template <typename GfxFamily>
inline void EncodeSurfaceState<GfxFamily>::encodeExtraBufferParams(EncodeSurfaceStateArgs &args) {
    auto surfaceState = reinterpret_cast<R_SURFACE_STATE *>(args.outMemory);
    encodeExtraCacheSettings(surfaceState, *args.gmmHelper->getHardwareInfo());
}

template <typename GfxFamily>
bool EncodeSurfaceState<GfxFamily>::doBindingTablePrefetch() {
    return true;
}

template <typename Family>
inline void EncodeSurfaceState<Family>::setCoherencyType(R_SURFACE_STATE *surfaceState, COHERENCY_TYPE coherencyType) {
    surfaceState->setCoherencyType(coherencyType);
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

    *cmd = localCmd;
}

template <typename GfxFamily>
void EncodeEnableRayTracing<GfxFamily>::programEnableRayTracing(LinearStream &commandStream, GraphicsAllocation &backBuffer) {
}

} // namespace NEO
