/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/kernel_commands.h"

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
size_t KernelCommandsHelper<GfxFamily>::getSizeRequiredCS() {
    return 2 * sizeof(typename GfxFamily::MEDIA_STATE_FLUSH) +
           sizeof(typename GfxFamily::MEDIA_INTERFACE_DESCRIPTOR_LOAD);
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
void KernelCommandsHelper<GfxFamily>::getCrossThreadData(
    uint32_t &sizeCrossThreadData,
    size_t &offsetCrossThreadData,
    Kernel &kernel,
    const bool &inlineDataProgrammingRequired,
    IndirectHeap &ioh,
    WALKER_TYPE<GfxFamily> *walkerCmd) {

    sizeCrossThreadData = kernel.getCrossThreadDataSize();
    offsetCrossThreadData = sendCrossThreadData(
        ioh,
        kernel);
}

template <typename GfxFamily>
size_t KernelCommandsHelper<GfxFamily>::getCrossThreadDataSize(
    uint32_t &sizeCrossThreadData,
    Kernel &kernel) {

    return sizeCrossThreadData;
}

template <typename GfxFamily>
bool KernelCommandsHelper<GfxFamily>::isRuntimeLocalIdsGenerationRequired(uint32_t workDim, size_t *gws, size_t *lws) {
    return true;
}

} // namespace OCLRT
