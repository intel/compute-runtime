/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/kernel_commands.h"
#include "runtime/kernel/kernel.h"

namespace OCLRT {

template <typename GfxFamily>
typename KernelCommandsHelper<GfxFamily>::INTERFACE_DESCRIPTOR_DATA *KernelCommandsHelper<GfxFamily>::getInterfaceDescriptor(
    const IndirectHeap &indirectHeap,
    uint64_t offsetInterfaceDescriptor,
    KernelCommandsHelper<GfxFamily>::INTERFACE_DESCRIPTOR_DATA *inlineInterfaceDescriptor) {
    return static_cast<INTERFACE_DESCRIPTOR_DATA *>(ptrOffset(indirectHeap.getCpuBase(), (size_t)offsetInterfaceDescriptor));
}

template <typename GfxFamily>
void KernelCommandsHelper<GfxFamily>::setAdditionalInfo(
    INTERFACE_DESCRIPTOR_DATA *pInterfaceDescriptor,
    const size_t &sizeCrossThreadData,
    const size_t &sizePerThreadData) {

    DEBUG_BREAK_IF((sizeCrossThreadData % sizeof(GRF)) != 0);
    auto numGrfCrossThreadData = static_cast<uint32_t>(sizeCrossThreadData / sizeof(GRF));
    DEBUG_BREAK_IF(numGrfCrossThreadData == 0);
    pInterfaceDescriptor->setCrossThreadConstantDataReadLength(numGrfCrossThreadData);

    DEBUG_BREAK_IF((sizePerThreadData % sizeof(GRF)) != 0);
    auto numGrfPerThreadData = static_cast<uint32_t>(sizePerThreadData / sizeof(GRF));

    // at least 1 GRF of perThreadData for each thread in a thread group when sizeCrossThreadData != 0
    numGrfPerThreadData = std::max(numGrfPerThreadData, 1u);
    pInterfaceDescriptor->setConstantIndirectUrbEntryReadLength(numGrfPerThreadData);
}

template <typename GfxFamily>
uint32_t KernelCommandsHelper<GfxFamily>::additionalSizeRequiredDsh() {
    return sizeof(INTERFACE_DESCRIPTOR_DATA);
}

template <typename GfxFamily>
size_t KernelCommandsHelper<GfxFamily>::getSizeRequiredCS(const Kernel *kernel) {
    size_t size = 2 * sizeof(typename GfxFamily::MEDIA_STATE_FLUSH) +
                  sizeof(typename GfxFamily::MEDIA_INTERFACE_DESCRIPTOR_LOAD);
    return size;
}

template <typename GfxFamily>
size_t KernelCommandsHelper<GfxFamily>::getSizeRequiredForCacheFlush(const Kernel *kernel, uint64_t postSyncAddress, uint64_t postSyncData) {
    return kernel->requiresCacheFlushCommand() ? sizeof(typename GfxFamily::PIPE_CONTROL) : 0;
}

template <typename GfxFamily>
void KernelCommandsHelper<GfxFamily>::sendMediaStateFlush(
    LinearStream &commandStream,
    size_t offsetInterfaceDescriptorData) {

    typedef typename GfxFamily::MEDIA_STATE_FLUSH MEDIA_STATE_FLUSH;
    auto pCmd = (MEDIA_STATE_FLUSH *)commandStream.getSpace(sizeof(MEDIA_STATE_FLUSH));
    *pCmd = GfxFamily::cmdInitMediaStateFlush;
    pCmd->setInterfaceDescriptorOffset((uint32_t)offsetInterfaceDescriptorData);
}

template <typename GfxFamily>
void KernelCommandsHelper<GfxFamily>::sendMediaInterfaceDescriptorLoad(
    LinearStream &commandStream,
    size_t offsetInterfaceDescriptorData,
    size_t sizeInterfaceDescriptorData) {
    {
        typedef typename GfxFamily::MEDIA_STATE_FLUSH MEDIA_STATE_FLUSH;
        auto pCmd = (MEDIA_STATE_FLUSH *)commandStream.getSpace(sizeof(MEDIA_STATE_FLUSH));
        *pCmd = GfxFamily::cmdInitMediaStateFlush;
    }

    {
        typedef typename GfxFamily::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
        auto pCmd = (MEDIA_INTERFACE_DESCRIPTOR_LOAD *)commandStream.getSpace(sizeof(MEDIA_INTERFACE_DESCRIPTOR_LOAD));
        *pCmd = GfxFamily::cmdInitMediaInterfaceDescriptorLoad;
        pCmd->setInterfaceDescriptorDataStartAddress((uint32_t)offsetInterfaceDescriptorData);
        pCmd->setInterfaceDescriptorTotalLength((uint32_t)sizeInterfaceDescriptorData);
    }
}

template <typename GfxFamily>
void KernelCommandsHelper<GfxFamily>::setKernelStartOffset(
    uint64_t &kernelStartOffset,
    bool kernelAllocation,
    const KernelInfo &kernelInfo,
    const bool &localIdsGenerationByRuntime,
    const bool &kernelUsesLocalIds,
    Kernel &kernel) {

    if (kernelAllocation) {
        kernelStartOffset = kernelInfo.getGraphicsAllocation()->getGpuAddressToPatch();
    }
    kernelStartOffset += kernel.getStartOffset();
}

template <typename GfxFamily>
void KernelCommandsHelper<GfxFamily>::programPerThreadData(
    size_t &sizePerThreadData,
    const bool &localIdsGenerationByRuntime,
    LinearStream &ioh,
    uint32_t &simd,
    uint32_t &numChannels,
    const size_t localWorkSize[3],
    Kernel &kernel,
    size_t &sizePerThreadDataTotal,
    size_t &localWorkItems) {

    sendPerThreadData(
        ioh,
        simd,
        numChannels,
        localWorkSize,
        kernel.getKernelInfo().workgroupDimensionsOrder,
        kernel.usesOnlyImages());

    updatePerThreadDataTotal(sizePerThreadData, simd, numChannels, sizePerThreadDataTotal, localWorkItems);
}

template <typename GfxFamily>
size_t KernelCommandsHelper<GfxFamily>::sendCrossThreadData(
    IndirectHeap &indirectHeap,
    Kernel &kernel,
    bool inlineDataProgrammingRequired,
    WALKER_TYPE<GfxFamily> *walkerCmd,
    uint32_t &sizeCrossThreadData) {
    indirectHeap.align(WALKER_TYPE<GfxFamily>::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);

    auto offsetCrossThreadData = indirectHeap.getUsed();
    char *pDest = static_cast<char *>(indirectHeap.getSpace(sizeCrossThreadData));
    memcpy_s(pDest, sizeCrossThreadData, kernel.getCrossThreadData(), sizeCrossThreadData);

    if (DebugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
        FlatBatchBufferHelper::fixCrossThreadDataInfo(kernel.getPatchInfoDataList(), offsetCrossThreadData, indirectHeap.getGraphicsAllocation()->getGpuAddress());
    }

    return offsetCrossThreadData + static_cast<size_t>(indirectHeap.getHeapGpuStartOffset());
}

template <typename GfxFamily>
bool KernelCommandsHelper<GfxFamily>::resetBindingTablePrefetch(Kernel &kernel) {
    return kernel.isSchedulerKernel || !doBindingTablePrefetch();
}

template <typename GfxFamily>
void KernelCommandsHelper<GfxFamily>::setInterfaceDescriptorOffset(
    WALKER_TYPE<GfxFamily> *walkerCmd,
    uint32_t &interfaceDescriptorIndex) {

    walkerCmd->setInterfaceDescriptorOffset(interfaceDescriptorIndex++);
}

template <typename GfxFamily>
bool KernelCommandsHelper<GfxFamily>::isRuntimeLocalIdsGenerationRequired(uint32_t workDim, size_t *gws, size_t *lws) {
    return true;
}

template <typename GfxFamily>
void KernelCommandsHelper<GfxFamily>::programCacheFlushAfterWalkerCommand(LinearStream *commandStream, const Kernel *kernel, uint64_t postSyncAddress, uint64_t postSyncData) {
    if (kernel->requiresCacheFlushCommand()) {
        using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
        auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(commandStream->getSpace(sizeof(PIPE_CONTROL)));
        *pipeControl = GfxFamily::cmdInitPipeControl;
        pipeControl->setCommandStreamerStallEnable(true);
        pipeControl->setDcFlushEnable(true);
    }
}
} // namespace OCLRT
