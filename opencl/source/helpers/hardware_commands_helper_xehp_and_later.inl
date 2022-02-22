/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/flat_batch_buffer_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/l3_range.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/string.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/kernel/kernel.h"

namespace NEO {

template <typename GfxFamily>
typename HardwareCommandsHelper<GfxFamily>::INTERFACE_DESCRIPTOR_DATA *HardwareCommandsHelper<GfxFamily>::getInterfaceDescriptor(
    const IndirectHeap &indirectHeap,
    uint64_t offsetInterfaceDescriptor,
    INTERFACE_DESCRIPTOR_DATA *inlineInterfaceDescriptor) {

    return inlineInterfaceDescriptor;
}

template <typename GfxFamily>
uint32_t HardwareCommandsHelper<GfxFamily>::additionalSizeRequiredDsh() {
    return 0u;
}

template <typename GfxFamily>
size_t HardwareCommandsHelper<GfxFamily>::getSizeRequiredCS() {
    return 0;
}

template <typename GfxFamily>
size_t HardwareCommandsHelper<GfxFamily>::getSizeRequiredForCacheFlush(const CommandQueue &commandQueue, const Kernel *kernel, uint64_t postSyncAddress) {
    UNRECOVERABLE_IF(true);
    return 0;
}

template <typename GfxFamily>
void HardwareCommandsHelper<GfxFamily>::sendMediaStateFlush(
    LinearStream &commandStream,
    size_t offsetInterfaceDescriptorData) {
}

template <typename GfxFamily>
void HardwareCommandsHelper<GfxFamily>::sendMediaInterfaceDescriptorLoad(
    LinearStream &commandStream,
    size_t offsetInterfaceDescriptorData,
    size_t sizeInterfaceDescriptorData) {
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
    if (localIdsGenerationByRuntime) {
        constexpr uint32_t grfSize = sizeof(typename GfxFamily::GRF);
        sendPerThreadData(
            ioh,
            simd,
            grfSize,
            numChannels,
            std::array<uint16_t, 3>{{static_cast<uint16_t>(localWorkSize[0]), static_cast<uint16_t>(localWorkSize[1]), static_cast<uint16_t>(localWorkSize[2])}},
            {{0u, 1u, 2u}},
            kernel.usesOnlyImages());

        updatePerThreadDataTotal(sizePerThreadData, simd, numChannels, sizePerThreadDataTotal, localWorkItems);
    }
}

template <typename GfxFamily>
size_t HardwareCommandsHelper<GfxFamily>::sendCrossThreadData(
    IndirectHeap &indirectHeap,
    Kernel &kernel,
    bool inlineDataProgrammingRequired,
    WALKER_TYPE *walkerCmd,
    uint32_t &sizeCrossThreadData) {

    indirectHeap.align(WALKER_TYPE::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);

    auto offsetCrossThreadData = indirectHeap.getUsed();
    char *dest = nullptr;
    char *src = kernel.getCrossThreadData();

    auto pImplicitArgs = kernel.getImplicitArgs();
    if (pImplicitArgs) {
        pImplicitArgs->localIdTablePtr = indirectHeap.getGraphicsAllocation()->getGpuAddress() + offsetCrossThreadData;

        const auto &kernelDescriptor = kernel.getDescriptor();
        const auto &hwInfo = kernel.getHardwareInfo();
        auto sizeForImplicitArgsProgramming = ImplicitArgsHelper::getSizeForImplicitArgsPatching(pImplicitArgs, kernelDescriptor, hwInfo);

        auto sizeForLocalIdsProgramming = sizeForImplicitArgsProgramming - sizeof(ImplicitArgs);
        offsetCrossThreadData += sizeForLocalIdsProgramming;

        auto ptrToPatchImplicitArgs = indirectHeap.getSpace(sizeForImplicitArgsProgramming);

        const auto &kernelAttributes = kernelDescriptor.kernelAttributes;
        uint32_t requiredWalkOrder = 0u;
        size_t localWorkSize[3] = {pImplicitArgs->localSizeX, pImplicitArgs->localSizeY, pImplicitArgs->localSizeZ};
        auto generationOfLocalIdsByRuntime = EncodeDispatchKernel<GfxFamily>::isRuntimeLocalIdsGenerationRequired(
            3,
            localWorkSize,
            std::array<uint8_t, 3>{
                {kernelAttributes.workgroupWalkOrder[0],
                 kernelAttributes.workgroupWalkOrder[1],
                 kernelAttributes.workgroupWalkOrder[2]}},
            kernelAttributes.flags.requiresWorkgroupWalkOrder,
            requiredWalkOrder,
            kernelDescriptor.kernelAttributes.simdSize);

        ImplicitArgsHelper::patchImplicitArgs(ptrToPatchImplicitArgs, *pImplicitArgs, kernelDescriptor, hwInfo, std::make_pair(generationOfLocalIdsByRuntime, requiredWalkOrder));
    }

    using InlineData = typename GfxFamily::INLINE_DATA;
    using GRF = typename GfxFamily::GRF;
    uint32_t inlineDataSize = sizeof(InlineData);
    uint32_t sizeToCopy = sizeCrossThreadData;
    if (inlineDataProgrammingRequired == true) {
        sizeToCopy = std::min(inlineDataSize, sizeCrossThreadData);
        dest = reinterpret_cast<char *>(walkerCmd->getInlineDataPointer());
        memcpy_s(dest, sizeToCopy, kernel.getCrossThreadData(), sizeToCopy);
        auto offset = std::min(inlineDataSize, sizeCrossThreadData);
        sizeCrossThreadData -= offset;
        src += offset;
    }

    if (sizeCrossThreadData > 0) {
        dest = static_cast<char *>(indirectHeap.getSpace(sizeCrossThreadData));
        memcpy_s(dest, sizeCrossThreadData, src, sizeCrossThreadData);
    }

    if (DebugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
        FlatBatchBufferHelper::fixCrossThreadDataInfo(kernel.getPatchInfoDataList(), offsetCrossThreadData, indirectHeap.getGraphicsAllocation()->getGpuAddress());
    }

    return offsetCrossThreadData + static_cast<size_t>(is64bit ? indirectHeap.getHeapGpuStartOffset() : indirectHeap.getHeapGpuBase());
}

template <typename GfxFamily>
bool HardwareCommandsHelper<GfxFamily>::resetBindingTablePrefetch() {
    return false;
}

template <typename GfxFamily>
void HardwareCommandsHelper<GfxFamily>::setInterfaceDescriptorOffset(
    WALKER_TYPE *walkerCmd,
    uint32_t &interfaceDescriptorIndex) {
}

template <typename GfxFamily>
void HardwareCommandsHelper<GfxFamily>::programCacheFlushAfterWalkerCommand(LinearStream *commandStream, const CommandQueue &commandQueue, const Kernel *kernel, [[maybe_unused]] uint64_t postSyncAddress) {
    // 1. make sure previous kernel finished
    PipeControlArgs args;
    auto &hardwareInfo = commandQueue.getDevice().getHardwareInfo();
    args.unTypedDataPortCacheFlush = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily).unTypedDataPortCacheFlushRequired();

    MemorySynchronizationCommands<GfxFamily>::addPipeControl(*commandStream, args);

    // 2. flush all affected L3 lines
    if constexpr (GfxFamily::isUsingL3Control) {
        StackVec<GraphicsAllocation *, 32> allocationsForCacheFlush;
        kernel->getAllocationsForCacheFlush(allocationsForCacheFlush);
        StackVec<L3Range, 128> subranges;
        for (GraphicsAllocation *alloc : allocationsForCacheFlush) {
            coverRangeExact(alloc->getGpuAddress(), alloc->getUnderlyingBufferSize(), subranges, GfxFamily::L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION);
        }
        for (size_t subrangeNumber = 0; subrangeNumber < subranges.size(); subrangeNumber += maxFlushSubrangeCount) {
            size_t rangeCount = subranges.size() <= subrangeNumber + maxFlushSubrangeCount ? subranges.size() - subrangeNumber : maxFlushSubrangeCount;
            Range<L3Range> range = CreateRange(subranges.begin() + subrangeNumber, rangeCount);
            uint64_t postSyncAddressToFlush = 0;
            if (rangeCount < maxFlushSubrangeCount || subranges.size() - subrangeNumber - maxFlushSubrangeCount == 0) {
                postSyncAddressToFlush = postSyncAddress;
            }

            flushGpuCache<GfxFamily>(commandStream, range, postSyncAddressToFlush, hardwareInfo);
        }
    }
}
} // namespace NEO
