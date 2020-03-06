/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/printf_handler.h"

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/program/print_formatter.h"

#include "level_zero/core/source/device_imp.h"

namespace L0 {

NEO::GraphicsAllocation *PrintfHandler::createPrintfBuffer(Device *device) {
    NEO::AllocationProperties properties(
        device->getRootDeviceIndex(), PrintfHandler::printfBufferSize, NEO::GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY);
    properties.alignment = MemoryConstants::pageSize64k;
    auto allocation = device->getDriverHandle()->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);

    *reinterpret_cast<uint32_t *>(allocation->getUnderlyingBuffer()) =
        PrintfHandler::printfSurfaceInitialDataSize;
    return allocation;
}

void PrintfHandler::printOutput(const KernelImmutableData *function,
                                NEO::GraphicsAllocation *printfBuffer, Device *device) {
    bool using32BitGpuPointers = function->getDescriptor().kernelAttributes.gpuPointerSize == 4u;
    NEO::PrintFormatter printfFormatter{static_cast<uint8_t *>(printfBuffer->getUnderlyingBuffer()),
                                        static_cast<uint32_t>(printfBuffer->getUnderlyingBufferSize()),
                                        using32BitGpuPointers,
                                        function->getDescriptor().kernelMetadata.printfStringsMap};
    printfFormatter.printKernelOutput();

    *reinterpret_cast<uint32_t *>(printfBuffer->getUnderlyingBuffer()) =
        PrintfHandler::printfSurfaceInitialDataSize;
}

size_t PrintfHandler::getPrintBufferSize() {
    return PrintfHandler::printfBufferSize;
}

} // namespace L0
