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

void CommandQueuePatchPreambleCounter::getPatchPreambleFullData(Device *device,
                                                                uint64_t &outCounterValue,
                                                                uint64_t *&outHostAddress,
                                                                uint64_t &outHostGpuAddress,
                                                                NEO::GraphicsAllocation *&outHostNodeGraphicsAllocation) {
    std::lock_guard<std::mutex> lock(this->mutex);
    if (this->hostCounterNode == nullptr) {
        auto tagAllocator = device->getHostInOrderCounterAllocator();
        this->hostCounterNode = tagAllocator->getTag();
        this->hostNodeCpuAddress = reinterpret_cast<uint64_t *>(this->hostCounterNode->getCpuBase());
        this->hostNodeGpuAddress = this->hostCounterNode->getGpuAddress();
        this->hostNodeAllocation = this->hostCounterNode->getBaseGraphicsAllocation()->getGraphicsAllocation(device->getRootDeviceIndex());
        memset(this->hostNodeCpuAddress, 0x0, tagAllocator->getTagSize());
    }
    outCounterValue = ++this->counter;
    outHostAddress = this->hostNodeCpuAddress;
    outHostGpuAddress = this->hostNodeGpuAddress;
    outHostNodeGraphicsAllocation = this->hostNodeAllocation;
}

} // namespace L0
