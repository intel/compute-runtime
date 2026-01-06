/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/helpers/flat_batch_buffer_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipe_control_args.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/helpers/hardware_commands_helper_base.inl"
#include "opencl/source/kernel/kernel.h"

namespace NEO {
using FamilyType = Gen12LpFamily;

template <typename GfxFamily>
template <typename InterfaceDescriptorType>
InterfaceDescriptorType *HardwareCommandsHelper<GfxFamily>::getInterfaceDescriptor(
    const IndirectHeap &indirectHeap,
    uint64_t offsetInterfaceDescriptor,
    InterfaceDescriptorType *inlineInterfaceDescriptor) {
    return static_cast<InterfaceDescriptorType *>(ptrOffset(indirectHeap.getCpuBase(), static_cast<size_t>(offsetInterfaceDescriptor)));
}

template <typename GfxFamily>
uint32_t HardwareCommandsHelper<GfxFamily>::additionalSizeRequiredDsh() {
    return sizeof(typename HardwareCommandsHelper::INTERFACE_DESCRIPTOR_DATA);
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

    DEBUG_BREAK_IF(indirectHeap.getUsed() % 64 != 0);

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

template <>
size_t HardwareCommandsHelper<FamilyType>::getSizeRequiredCS() {
    size_t size = 2 * sizeof(typename FamilyType::MEDIA_STATE_FLUSH) +
                  sizeof(typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD);
    return size;
}

} // namespace NEO

template struct NEO::HardwareCommandsHelperWithHeap<NEO::FamilyType>;

#include "opencl/source/helpers/enable_hardware_commands_helper_gpgpu.inl"
