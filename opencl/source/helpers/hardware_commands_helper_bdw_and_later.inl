/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/pipe_control_args.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/kernel/kernel.h"

namespace NEO {

template <typename GfxFamily>
typename HardwareCommandsHelper<GfxFamily>::INTERFACE_DESCRIPTOR_DATA *HardwareCommandsHelper<GfxFamily>::getInterfaceDescriptor(
    const IndirectHeap &indirectHeap,
    uint64_t offsetInterfaceDescriptor,
    HardwareCommandsHelper<GfxFamily>::INTERFACE_DESCRIPTOR_DATA *inlineInterfaceDescriptor) {
    return static_cast<INTERFACE_DESCRIPTOR_DATA *>(ptrOffset(indirectHeap.getCpuBase(), (size_t)offsetInterfaceDescriptor));
}

template <typename GfxFamily>
uint32_t HardwareCommandsHelper<GfxFamily>::additionalSizeRequiredDsh() {
    return sizeof(INTERFACE_DESCRIPTOR_DATA);
}

template <typename GfxFamily>
size_t HardwareCommandsHelper<GfxFamily>::getSizeRequiredCS() {
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

    using MEDIA_STATE_FLUSH = typename GfxFamily::MEDIA_STATE_FLUSH;
    auto pCmd = commandStream.getSpaceForCmd<MEDIA_STATE_FLUSH>();
    MEDIA_STATE_FLUSH cmd = GfxFamily::cmdInitMediaStateFlush;

    cmd.setInterfaceDescriptorOffset(static_cast<uint32_t>(offsetInterfaceDescriptorData));
    *pCmd = cmd;
}

template <typename GfxFamily>
void HardwareCommandsHelper<GfxFamily>::sendMediaInterfaceDescriptorLoad(
    LinearStream &commandStream,
    size_t offsetInterfaceDescriptorData,
    size_t sizeInterfaceDescriptorData) {
    {
        using MEDIA_STATE_FLUSH = typename GfxFamily::MEDIA_STATE_FLUSH;
        auto pCmd = commandStream.getSpaceForCmd<MEDIA_STATE_FLUSH>();
        *pCmd = GfxFamily::cmdInitMediaStateFlush;
    }

    {
        using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename GfxFamily::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
        auto pCmd = commandStream.getSpaceForCmd<MEDIA_INTERFACE_DESCRIPTOR_LOAD>();
        MEDIA_INTERFACE_DESCRIPTOR_LOAD cmd = GfxFamily::cmdInitMediaInterfaceDescriptorLoad;
        cmd.setInterfaceDescriptorDataStartAddress(static_cast<uint32_t>(offsetInterfaceDescriptorData));
        cmd.setInterfaceDescriptorTotalLength(static_cast<uint32_t>(sizeInterfaceDescriptorData));
        *pCmd = cmd;
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
    size_t &localWorkItems,
    uint32_t rootDeviceIndex) {

    uint32_t grfSize = sizeof(typename GfxFamily::GRF);

    sendPerThreadData(
        ioh,
        simd,
        grfSize,
        numChannels,
        std::array<uint16_t, 3>{{static_cast<uint16_t>(localWorkSize[0]), static_cast<uint16_t>(localWorkSize[1]), static_cast<uint16_t>(localWorkSize[2])}},
        std::array<uint8_t, 3>{{kernel.getKernelInfo().kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[0],
                                kernel.getKernelInfo().kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[1],
                                kernel.getKernelInfo().kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[2]}},
        kernel.usesOnlyImages());

    updatePerThreadDataTotal(sizePerThreadData, simd, numChannels, sizePerThreadDataTotal, localWorkItems);
}

template <typename GfxFamily>
size_t HardwareCommandsHelper<GfxFamily>::sendCrossThreadData(
    IndirectHeap &indirectHeap,
    Kernel &kernel,
    bool inlineDataProgrammingRequired,
    WALKER_TYPE *walkerCmd,
    uint32_t &sizeCrossThreadData) {
    indirectHeap.align(WALKER_TYPE::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);

    auto pImplicitArgs = kernel.getImplicitArgs();
    if (pImplicitArgs) {
        const auto &kernelDescriptor = kernel.getDescriptor();
        const auto &hwInfo = kernel.getHardwareInfo();
        auto sizeForImplicitArgsProgramming = ImplicitArgsHelper::getSizeForImplicitArgsPatching(pImplicitArgs, kernelDescriptor, hwInfo);

        auto implicitArgsGpuVA = indirectHeap.getGraphicsAllocation()->getGpuAddress() + indirectHeap.getUsed();
        auto ptrToPatchImplicitArgs = indirectHeap.getSpace(sizeForImplicitArgsProgramming);
        ImplicitArgsHelper::patchImplicitArgs(ptrToPatchImplicitArgs, *pImplicitArgs, kernelDescriptor, hwInfo, {});

        auto implicitArgsCrossThreadPtr = ptrOffset(reinterpret_cast<uint64_t *>(kernel.getCrossThreadData()), kernelDescriptor.payloadMappings.implicitArgs.implicitArgsBuffer);
        *implicitArgsCrossThreadPtr = implicitArgsGpuVA;
    }
    auto offsetCrossThreadData = indirectHeap.getUsed();
    char *pDest = nullptr;

    pDest = static_cast<char *>(indirectHeap.getSpace(sizeCrossThreadData));
    memcpy_s(pDest, sizeCrossThreadData, kernel.getCrossThreadData(), sizeCrossThreadData);

    if (DebugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
        FlatBatchBufferHelper::fixCrossThreadDataInfo(kernel.getPatchInfoDataList(), offsetCrossThreadData, indirectHeap.getGraphicsAllocation()->getGpuAddress());
    }

    return offsetCrossThreadData + static_cast<size_t>(indirectHeap.getHeapGpuStartOffset());
}

template <typename GfxFamily>
void HardwareCommandsHelper<GfxFamily>::setInterfaceDescriptorOffset(
    WALKER_TYPE *walkerCmd,
    uint32_t &interfaceDescriptorIndex) {

    walkerCmd->setInterfaceDescriptorOffset(interfaceDescriptorIndex++);
}

template <typename GfxFamily>
void HardwareCommandsHelper<GfxFamily>::programCacheFlushAfterWalkerCommand(LinearStream *commandStream, const CommandQueue &commandQueue, const Kernel *kernel, uint64_t postSyncAddress) {
    const auto &hwInfo = commandQueue.getDevice().getHardwareInfo();
    PipeControlArgs args;
    args.dcFlushEnable = MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, hwInfo);
    MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(*commandStream, args);
}

} // namespace NEO
