/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/assert_handler/assert_handler.h"

#include "shared/source/device/device.h"
#include "shared/source/helpers/abort.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/program/print_formatter.h"

namespace NEO {
AssertHandler::AssertHandler(Device *device) : device(device) {
    auto rootDeviceIndex = device->getRootDeviceIndex();
    assertBuffer = device->getMemoryManager()->allocateGraphicsMemoryWithProperties({rootDeviceIndex, assertBufferSize, AllocationType::assertBuffer, device->getDeviceBitfield()});

    AssertBufferHeader initialHeader = {};
    initialHeader.size = assertBufferSize;
    initialHeader.flags = 0;
    initialHeader.begin = sizeof(AssertBufferHeader);
    *reinterpret_cast<AssertBufferHeader *>(assertBuffer->getUnderlyingBuffer()) = initialHeader;
}

AssertHandler::~AssertHandler() {
    device->getMemoryManager()->freeGraphicsMemory(assertBuffer);
}

bool AssertHandler::checkAssert() const {
    return reinterpret_cast<AssertBufferHeader *>(assertBuffer->getUnderlyingBuffer())->flags != 0;
}

void AssertHandler::printMessage() const {

    auto messageBuffer = static_cast<uint8_t *>(assertBuffer->getUnderlyingBuffer());
    auto messageBufferSize = static_cast<uint32_t>(assertBuffer->getUnderlyingBufferSize());

    NEO::PrintFormatter printfFormatter{
        messageBuffer,
        messageBufferSize,
        false,
        nullptr};
    printfFormatter.setInitialOffset(offsetof(AssertBufferHeader, begin));

    printToStderr("AssertHandler::printMessage\n");
    printfFormatter.printKernelOutput([](char *str) { printToStderr(str); });
}

void AssertHandler::printAssertAndAbort() {
    std::lock_guard<std::mutex> lock(this->mtx);
    if (checkAssert()) {
        printMessage();
        abortExecution();
    }
}
} // namespace NEO
