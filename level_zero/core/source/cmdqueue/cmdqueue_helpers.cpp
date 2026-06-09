/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdqueue/cmdqueue_helpers.h"

#include "shared/source/utilities/tag_allocator.h"

#include "level_zero/core/source/device/device.h"

namespace L0 {

CommandQueuePatchPreambleCounter::~CommandQueuePatchPreambleCounter() {
    if (this->hostCounterNode != nullptr) {
        this->hostCounterNode->returnTag();
    }
}

void CommandQueuePatchPreambleCounter::getPatchPreambleHostCounter(Device *device, uint64_t &outCounterValue, uint64_t *&outHostAddress) {
    std::lock_guard<std::mutex> lock(this->mutex);
    if (this->hostCounterNode == nullptr) {
        this->hostCounterNode = device->getHostInOrderCounterAllocator()->getTag();
        this->hostAddress = reinterpret_cast<uint64_t *>(this->hostCounterNode->getCpuBase());
        this->deviceAddress = this->hostCounterNode->getGpuAddress();
        this->allocation = this->hostCounterNode->getBaseGraphicsAllocation()->getGraphicsAllocation(device->getRootDeviceIndex());
    }
    outCounterValue = ++this->counter;
    outHostAddress = this->hostAddress;
}

void CommandQueuePatchPreambleCounter::getPatchPreambleDeviceData(NEO::GraphicsAllocation *&outAllocation, uint64_t &outDeviceAddress) {
    outDeviceAddress = this->deviceAddress;
    outAllocation = this->allocation;
}

} // namespace L0
