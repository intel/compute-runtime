/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/flat_batch_buffer_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
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
template <typename WalkerType>
size_t HardwareCommandsHelper<GfxFamily>::sendCrossThreadData(
    IndirectHeap &indirectHeap,
    Kernel &kernel,
    bool inlineDataProgrammingRequired,
    WalkerType *walkerCmd,
    uint32_t &sizeCrossThreadData,
    uint64_t scratchAddress,
    const RootDeviceEnvironment &rootDeviceEnvironment) {
    indirectHeap.align(GfxFamily::cacheLineSize);

    auto pImplicitArgs = kernel.getImplicitArgs();
    if (pImplicitArgs) {
        const auto &kernelDescriptor = kernel.getDescriptor();

        auto isHwLocalIdGeneration = false;
        auto sizeForImplicitArgsProgramming = ImplicitArgsHelper::getSizeForImplicitArgsPatching(pImplicitArgs, kernelDescriptor, isHwLocalIdGeneration, rootDeviceEnvironment);

        auto implicitArgsGpuVA = indirectHeap.getGraphicsAllocation()->getGpuAddress() + indirectHeap.getUsed();
        auto ptrToPatchImplicitArgs = indirectHeap.getSpace(sizeForImplicitArgsProgramming);

        ImplicitArgsHelper::patchImplicitArgs(ptrToPatchImplicitArgs, *pImplicitArgs, kernelDescriptor, {}, rootDeviceEnvironment, nullptr);

        auto implicitArgsCrossThreadPtr = ptrOffset(reinterpret_cast<uint64_t *>(kernel.getCrossThreadData()), kernelDescriptor.payloadMappings.implicitArgs.implicitArgsBuffer);
        *implicitArgsCrossThreadPtr = implicitArgsGpuVA;
    }
    auto offsetCrossThreadData = indirectHeap.getUsed();
    char *pDest = nullptr;

    pDest = static_cast<char *>(indirectHeap.getSpace(sizeCrossThreadData));
    memcpy_s(pDest, sizeCrossThreadData, kernel.getCrossThreadData(), sizeCrossThreadData);

    if (debugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
        FlatBatchBufferHelper::fixCrossThreadDataInfo(kernel.getPatchInfoDataList(), offsetCrossThreadData, indirectHeap.getGraphicsAllocation()->getGpuAddress());
    }

    return offsetCrossThreadData + static_cast<size_t>(indirectHeap.getHeapGpuStartOffset());
}

template <typename GfxFamily>
template <typename WalkerType>
void HardwareCommandsHelper<GfxFamily>::setInterfaceDescriptorOffset(
    WalkerType *walkerCmd,
    uint32_t &interfaceDescriptorIndex) {

    walkerCmd->setInterfaceDescriptorOffset(interfaceDescriptorIndex++);
}

} // namespace NEO
