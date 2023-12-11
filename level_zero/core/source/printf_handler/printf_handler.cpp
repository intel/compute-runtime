/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/printf_handler/printf_handler.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/blit_properties.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/program/print_formatter.h"

#include "level_zero/core/source/device/device_imp.h"

namespace L0 {

NEO::GraphicsAllocation *PrintfHandler::createPrintfBuffer(Device *device) {
    NEO::AllocationProperties properties(
        device->getRootDeviceIndex(), PrintfHandler::printfBufferSize, NEO::AllocationType::printfSurface, device->getNEODevice()->getDeviceBitfield());
    properties.alignment = MemoryConstants::pageSize64k;
    auto allocation = device->getNEODevice()->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);

    *reinterpret_cast<uint32_t *>(allocation->getUnderlyingBuffer()) =
        PrintfHandler::printfSurfaceInitialDataSize;
    return allocation;
}

void PrintfHandler::printOutput(const KernelImmutableData *kernelData,
                                NEO::GraphicsAllocation *printfBuffer, Device *device, bool useInternalBlitter) {
    bool using32BitGpuPointers = kernelData->getDescriptor().kernelAttributes.gpuPointerSize == 4u;
    auto usesStringMap = kernelData->getDescriptor().kernelAttributes.usesStringMap();

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

            const auto newTaskCount = bcsEngine->commandStreamReceiver->flushBcsTask(blitPropertiesContainer, true, false, *selectedDevice);
            if (newTaskCount == NEO::CompletionStamp::gpuHang) {
                PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Failed to copy printf buffer.\n", "");
                printfOutputBuffer = static_cast<uint8_t *>(printfBuffer->getUnderlyingBuffer());
            }
        }
    }

    NEO::PrintFormatter printfFormatter{
        printfOutputBuffer,
        printfOutputSize,
        using32BitGpuPointers,
        usesStringMap ? &kernelData->getDescriptor().kernelMetadata.printfStringsMap : nullptr};
    printfFormatter.printKernelOutput();

    *reinterpret_cast<uint32_t *>(printfBuffer->getUnderlyingBuffer()) =
        PrintfHandler::printfSurfaceInitialDataSize;
}

size_t PrintfHandler::getPrintBufferSize() {
    return PrintfHandler::printfBufferSize;
}

} // namespace L0
