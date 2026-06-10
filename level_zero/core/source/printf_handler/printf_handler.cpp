/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/printf_handler/printf_handler.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/blit_properties.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/program/print_formatter.h"

#include "level_zero/core/source/device/device.h"

namespace L0 {

NEO::GraphicsAllocation *PrintfHandler::createPrintfBuffer(Device *device) {
    NEO::AllocationProperties properties(
        device->getRootDeviceIndex(), PrintfHandler::printfBufferSize, NEO::AllocationType::printfSurface, device->getNEODevice()->getDeviceBitfield());
    properties.alignment = MemoryConstants::pageSize64k;

    DEBUG_BREAK_IF(device->getNEODevice()->getProductHelper().is2MBLocalMemAlignmentEnabled() &&
                   !isAligned(properties.size, MemoryConstants::pageSize2M));

    auto allocation = device->getNEODevice()->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);

    *reinterpret_cast<uint32_t *>(allocation->getUnderlyingBuffer()) =
        PrintfHandler::printfSurfaceInitialDataSize;
    return allocation;
}

void PrintfHandler::printOutput(const KernelImmutableData *kernelData,
                                NEO::GraphicsAllocation *printfBuffer, Device *device, bool useInternalBlitter,
                                uint32_t &alreadyPrintedSize) {
    bool using32BitGpuPointers = kernelData->getDescriptor().kernelAttributes.gpuPointerSize == 4u;

    auto printfOutputBuffer = static_cast<uint8_t *>(printfBuffer->getUnderlyingBuffer());
    auto printfOutputSize = static_cast<uint32_t>(printfBuffer->getUnderlyingBufferSize());
    std::unique_ptr<uint8_t[]> printfOutputTemporary;

    if (useInternalBlitter) {
        auto selectedDevice = device->getNEODevice()->getNearestGenericSubDevice(0);
        auto &selectorCopyEngine = selectedDevice->getSelectorCopyEngine();
        auto deviceBitfield = selectedDevice->getDeviceBitfield();
        const auto internalUsage = true;
        auto bcsEngineType = NEO::EngineHelpers::getBcsEngineType(selectedDevice->getRootDeviceEnvironment(), deviceBitfield, selectorCopyEngine, internalUsage);
        auto bcsEngineUsage = NEO::EngineUsage::internal;
        auto bcsEngine = selectedDevice->tryGetEngine(bcsEngineType, bcsEngineUsage);

        if (bcsEngine) {
            printfOutputTemporary = std::make_unique<uint8_t[]>(printfOutputSize);
            printfOutputBuffer = printfOutputTemporary.get();

            NEO::BlitPropertiesContainer blitPropertiesContainer;
            blitPropertiesContainer.push_back(
                NEO::BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::bufferToHostPtr,
                                                                     *bcsEngine->commandStreamReceiver, printfBuffer, nullptr,
                                                                     printfOutputBuffer,
                                                                     printfBuffer->getGpuAddress(),
                                                                     0, 0, 0, Vec3<size_t>(printfOutputSize, 0, 0), 0, 0, 0, 0));

            const auto newTaskCount = bcsEngine->commandStreamReceiver->flushBcsTask(blitPropertiesContainer, true, *selectedDevice);
            if (newTaskCount == NEO::CompletionStamp::gpuHang) {
                PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Failed to copy printf buffer.\n", "");
                printfOutputBuffer = static_cast<uint8_t *>(printfBuffer->getUnderlyingBuffer());
            }
        }
    }

    uint32_t bytesWritten = *reinterpret_cast<const uint32_t *>(printfOutputBuffer); // NOLINT(clang-analyzer-core.uninitialized.Assign)
    bytesWritten = std::min(bytesWritten, printfOutputSize);
    if (bytesWritten <= alreadyPrintedSize) {
        return;
    }

    NEO::PrintFormatter printfFormatter{
        printfOutputBuffer,
        printfOutputSize,
        using32BitGpuPointers};
    printfFormatter.setStartParsingOffset(alreadyPrintedSize);
    printfFormatter.printKernelOutput();

    alreadyPrintedSize = bytesWritten;

    // Rewind only when full: the GPU has stopped writing, so this cannot race a live launch.
    const uint32_t minEntrySize = using32BitGpuPointers ? sizeof(uint32_t) : sizeof(uint64_t);
    if (bytesWritten + minEntrySize > printfOutputSize) {
        rewindPrintfBuffer(printfBuffer, device, useInternalBlitter);
        alreadyPrintedSize = 0;
    }
}

void PrintfHandler::rewindPrintfBuffer(NEO::GraphicsAllocation *printfBuffer, Device *device, bool useInternalBlitter) {
    uint32_t initialValue = PrintfHandler::printfSurfaceInitialDataSize;

    if (useInternalBlitter) {
        auto selectedDevice = device->getNEODevice()->getNearestGenericSubDevice(0);
        auto &selectorCopyEngine = selectedDevice->getSelectorCopyEngine();
        auto deviceBitfield = selectedDevice->getDeviceBitfield();
        auto bcsEngineType = NEO::EngineHelpers::getBcsEngineType(selectedDevice->getRootDeviceEnvironment(), deviceBitfield, selectorCopyEngine, true);
        auto bcsEngine = selectedDevice->tryGetEngine(bcsEngineType, NEO::EngineUsage::internal);

        if (bcsEngine) {
            NEO::BlitPropertiesContainer blitPropertiesContainer;
            blitPropertiesContainer.push_back(
                NEO::BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::hostPtrToBuffer,
                                                                     *bcsEngine->commandStreamReceiver, printfBuffer, nullptr,
                                                                     &initialValue,
                                                                     printfBuffer->getGpuAddress(),
                                                                     0, 0, 0, Vec3<size_t>(sizeof(initialValue), 0, 0), 0, 0, 0, 0));

            auto newTaskCount = bcsEngine->commandStreamReceiver->flushBcsTask(blitPropertiesContainer, true, *selectedDevice);
            if (newTaskCount != NEO::CompletionStamp::gpuHang) {
                return;
            }
        }
    }

    *reinterpret_cast<uint32_t *>(printfBuffer->getUnderlyingBuffer()) = initialValue;
}

size_t PrintfHandler::getPrintBufferSize() {
    return PrintfHandler::printfBufferSize;
}

} // namespace L0
