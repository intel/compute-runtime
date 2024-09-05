/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/sync_buffer_handler.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"

#include <cstring>

namespace NEO {

SyncBufferHandler::~SyncBufferHandler() {
    memoryManager.checkGpuUsageAndDestroyGraphicsAllocations(graphicsAllocation);
};
SyncBufferHandler::SyncBufferHandler(Device &device)
    : device(device), memoryManager(*device.getMemoryManager()) {

    allocateNewBuffer();
}

void SyncBufferHandler::makeResident(CommandStreamReceiver &csr) {
    csr.makeResident(*graphicsAllocation);
}

void SyncBufferHandler::allocateNewBuffer() {
    AllocationProperties allocationProperties{device.getRootDeviceIndex(), true, bufferSize,
                                              AllocationType::syncBuffer,
                                              (device.getNumGenericSubDevices() > 1u), /* multiOsContextCapable */
                                              false, device.getDeviceBitfield()};
    graphicsAllocation = memoryManager.allocateGraphicsMemoryWithProperties(allocationProperties);
    UNRECOVERABLE_IF(graphicsAllocation == nullptr);

    auto cpuPointer = graphicsAllocation->getUnderlyingBuffer();
    std::memset(cpuPointer, 0, bufferSize);
}

std::pair<GraphicsAllocation *, size_t> SyncBufferHandler::obtainAllocationAndOffset(size_t requiredSize) {
    std::lock_guard<std::mutex> guard(this->mutex);

    bool isCurrentBufferFull = (usedBufferSize + requiredSize > bufferSize);
    if (isCurrentBufferFull) {
        memoryManager.checkGpuUsageAndDestroyGraphicsAllocations(graphicsAllocation);
        allocateNewBuffer();
        usedBufferSize = 0;
    }

    std::pair<GraphicsAllocation *, size_t> allocationAndOffset = {graphicsAllocation, usedBufferSize};

    usedBufferSize += requiredSize;

    return allocationAndOffset;
}

} // namespace NEO
