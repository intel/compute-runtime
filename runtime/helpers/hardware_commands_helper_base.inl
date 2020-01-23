/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/hw_helper.h"
#include "runtime/helpers/hardware_commands_helper.h"
#include "runtime/kernel/kernel.h"

namespace NEO {

template <typename GfxFamily>
typename HardwareCommandsHelper<GfxFamily>::INTERFACE_DESCRIPTOR_DATA *HardwareCommandsHelper<GfxFamily>::getInterfaceDescriptor(
    const IndirectHeap &indirectHeap,
    uint64_t offsetInterfaceDescriptor,
    HardwareCommandsHelper<GfxFamily>::INTERFACE_DESCRIPTOR_DATA *inlineInterfaceDescriptor) {
    return static_cast<INTERFACE_DESCRIPTOR_DATA *>(ptrOffset(indirectHeap.getCpuBase(), (size_t)offsetInterfaceDescriptor));
}

template <typename GfxFamily>
void HardwareCommandsHelper<GfxFamily>::setAdditionalInfo(
    INTERFACE_DESCRIPTOR_DATA *pInterfaceDescriptor,
    const Kernel &kernel,
    const size_t &sizeCrossThreadData,
    const size_t &sizePerThreadData,
    const uint32_t threadsPerThreadGroup) {
    auto grfSize = sizeof(typename GfxFamily::GRF);
    DEBUG_BREAK_IF((sizeCrossThreadData % grfSize) != 0);
    auto numGrfCrossThreadData = static_cast<uint32_t>(sizeCrossThreadData / grfSize);
    DEBUG_BREAK_IF(numGrfCrossThreadData == 0);
    pInterfaceDescriptor->setCrossThreadConstantDataReadLength(numGrfCrossThreadData);

    DEBUG_BREAK_IF((sizePerThreadData % grfSize) != 0);
    auto numGrfPerThreadData = static_cast<uint32_t>(sizePerThreadData / grfSize);

    // at least 1 GRF of perThreadData for each thread in a thread group when sizeCrossThreadData != 0
    numGrfPerThreadData = std::max(numGrfPerThreadData, 1u);
    pInterfaceDescriptor->setConstantIndirectUrbEntryReadLength(numGrfPerThreadData);
}

template <typename GfxFamily>
uint32_t HardwareCommandsHelper<GfxFamily>::additionalSizeRequiredDsh() {
    return sizeof(INTERFACE_DESCRIPTOR_DATA);
}

template <typename GfxFamily>
size_t HardwareCommandsHelper<GfxFamily>::getSizeRequiredCS(const Kernel *kernel) {
    size_t size = 2 * sizeof(typename GfxFamily::MEDIA_STATE_FLUSH) +
                  sizeof(typename GfxFamily::MEDIA_INTERFACE_DESCRIPTOR_LOAD);
    return size;
}

template <typename GfxFamily>
size_t HardwareCommandsHelper<GfxFamily>::getSizeRequiredForCacheFlush(const CommandQueue &commandQueue, const Kernel *kernel, uint64_t postSyncAddress) {
    return kernel->requiresCacheFlushCommand(commandQueue) ? sizeof(typename GfxFamily::PIPE_CONTROL) : 0;
}

template <typename GfxFamily>
void HardwareCommandsHelper<GfxFamily>::sendMediaStateFlush(
    LinearStream &commandStream,
    size_t offsetInterfaceDescriptorData) {

    typedef typename GfxFamily::MEDIA_STATE_FLUSH MEDIA_STATE_FLUSH;
    auto pCmd = (MEDIA_STATE_FLUSH *)commandStream.getSpace(sizeof(MEDIA_STATE_FLUSH));
    *pCmd = GfxFamily::cmdInitMediaStateFlush;
    pCmd->setInterfaceDescriptorOffset((uint32_t)offsetInterfaceDescriptorData);
}

template <typename GfxFamily>
void HardwareCommandsHelper<GfxFamily>::sendMediaInterfaceDescriptorLoad(
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
void HardwareCommandsHelper<GfxFamily>::programPerThreadData(
    size_t &sizePerThreadData,
    const bool &localIdsGenerationByRuntime,
    LinearStream &ioh,
    uint32_t &simd,
    uint32_t &numChannels,
    const size_t localWorkSize[3],
    Kernel &kernel,
    size_t &sizePerThreadDataTotal,
    size_t &localWorkItems) {

    uint32_t grfSize = sizeof(typename GfxFamily::GRF);

    sendPerThreadData(
        ioh,
        simd,
        grfSize,
        numChannels,
        localWorkSize,
        kernel.getKernelInfo().workgroupDimensionsOrder,
        kernel.usesOnlyImages());

    updatePerThreadDataTotal(sizePerThreadData, simd, numChannels, sizePerThreadDataTotal, localWorkItems);
}

template <typename GfxFamily>
size_t HardwareCommandsHelper<GfxFamily>::sendCrossThreadData(
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
bool HardwareCommandsHelper<GfxFamily>::resetBindingTablePrefetch(Kernel &kernel) {
    return kernel.isSchedulerKernel || !doBindingTablePrefetch();
}

template <typename GfxFamily>
void HardwareCommandsHelper<GfxFamily>::setInterfaceDescriptorOffset(
    WALKER_TYPE<GfxFamily> *walkerCmd,
    uint32_t &interfaceDescriptorIndex) {

    walkerCmd->setInterfaceDescriptorOffset(interfaceDescriptorIndex++);
}

template <typename GfxFamily>
void HardwareCommandsHelper<GfxFamily>::programCacheFlushAfterWalkerCommand(LinearStream *commandStream, const CommandQueue &commandQueue, const Kernel *kernel, uint64_t postSyncAddress) {
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(commandStream->getSpace(sizeof(PIPE_CONTROL)));
    *pipeControl = GfxFamily::cmdInitPipeControl;
    pipeControl->setCommandStreamerStallEnable(true);
    pipeControl->setDcFlushEnable(true);
}

template <typename GfxFamily>
void HardwareCommandsHelper<GfxFamily>::appendMiFlushDw(typename GfxFamily::MI_FLUSH_DW *miFlushDwCmd) {}

template <typename GfxFamily>
void HardwareCommandsHelper<GfxFamily>::programBarrierEnable(INTERFACE_DESCRIPTOR_DATA *pInterfaceDescriptor, uint32_t value, const HardwareInfo &hwInfo) {
    pInterfaceDescriptor->setBarrierEnable(value);
}

template <typename GfxFamily>
void HardwareCommandsHelper<GfxFamily>::adjustInterfaceDescriptorData(INTERFACE_DESCRIPTOR_DATA *pInterfaceDescriptor, const HardwareInfo &hwInfo) {}
} // namespace NEO
