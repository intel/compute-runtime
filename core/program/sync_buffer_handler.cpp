/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/program/sync_buffer_handler.h"

#include "core/memory_manager/graphics_allocation.h"
#include "core/memory_manager/memory_manager.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/kernel/kernel.h"

namespace NEO {

SyncBufferHandler::~SyncBufferHandler() {
    memoryManager.checkGpuUsageAndDestroyGraphicsAllocations(graphicsAllocation);
};
SyncBufferHandler::SyncBufferHandler(Device &device)
    : device(device), memoryManager(*device.getMemoryManager()) {

    allocateNewBuffer();
}

void SyncBufferHandler::prepareForEnqueue(size_t workGroupsCount, Kernel &kernel, CommandStreamReceiver &csr) {
    auto requiredSize = workGroupsCount;
    std::lock_guard<std::mutex> guard(this->mutex);

    bool isCurrentBufferFull = (usedBufferSize + requiredSize > bufferSize);
    if (isCurrentBufferFull) {
        memoryManager.checkGpuUsageAndDestroyGraphicsAllocations(graphicsAllocation);
        allocateNewBuffer();
        usedBufferSize = 0;
    }

    kernel.patchSyncBuffer(device, graphicsAllocation, usedBufferSize);
    csr.makeResident(*graphicsAllocation);

    usedBufferSize += requiredSize;
}

void SyncBufferHandler::allocateNewBuffer() {
    AllocationProperties allocationProperties{device.getRootDeviceIndex(), true, bufferSize,
                                              GraphicsAllocation::AllocationType::LINEAR_STREAM,
                                              false, false, device.getDeviceBitfield()};
    graphicsAllocation = memoryManager.allocateGraphicsMemoryWithProperties(allocationProperties);
    UNRECOVERABLE_IF(graphicsAllocation == nullptr);

    auto cpuPointer = graphicsAllocation->getUnderlyingBuffer();
    std::memset(cpuPointer, 0, bufferSize);
}

} // namespace NEO
