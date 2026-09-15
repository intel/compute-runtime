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
        this->deviceCounterNode->returnTag();
    }
}

void CommandQueuePatchPreambleCounter::getPatchPreambleFullData(Device *device,
                                                                uint64_t &outCounterValue,
                                                                uint64_t *&outHostAddress,
                                                                uint64_t &outHostGpuAddress,
                                                                NEO::GraphicsAllocation *&outHostNodeGraphicsAllocation,
                                                                uint64_t &outDeviceGpuAddress,
                                                                NEO::GraphicsAllocation *&outDeviceNodeGraphicsAllocation) {
    std::lock_guard<std::mutex> lock(this->mutex);
    if (this->hostCounterNode == nullptr) {
        auto tagAllocator = device->getHostInOrderCounterAllocator();
        this->hostCounterNode = tagAllocator->getTag();
        this->hostNodeCpuAddress = reinterpret_cast<uint64_t *>(this->hostCounterNode->getCpuBase());
        this->hostNodeGpuAddress = this->hostCounterNode->getGpuAddress();
        this->hostNodeAllocation = this->hostCounterNode->getBaseGraphicsAllocation()->getGraphicsAllocation(device->getRootDeviceIndex());
        memset(this->hostNodeCpuAddress, 0x0, tagAllocator->getTagSize());

        auto deviceTagAllocator = device->getDeviceInOrderCounterAllocator();
        this->deviceCounterNode = deviceTagAllocator->getTag();
        this->deviceNodeGpuAddress = this->deviceCounterNode->getGpuAddress();
        this->deviceNodeAllocation = this->deviceCounterNode->getBaseGraphicsAllocation()->getGraphicsAllocation(device->getRootDeviceIndex());
        this->deviceNodeSize = deviceTagAllocator->getTagSize();
        memset(this->deviceCounterNode->getCpuBase(), 0x0, this->deviceNodeSize);
    }

    ++this->counter;
    if (this->use32bSemaphore) {
        if (0 == getLowPart(this->counter)) {
            ++this->counter;
            if (this->offset == 0) {
                this->offset = this->deviceNodeSize / 2;
            } else {
                this->offset = 0;
            }
            // now reset partial device node
            void *partialNode = ptrOffset(this->deviceCounterNode->getCpuBase(), this->offset);
            memset(partialNode, 0x0, this->deviceNodeSize / 2);
        }
    }
    outCounterValue = this->counter;
    outHostAddress = this->hostNodeCpuAddress;
    outHostGpuAddress = this->hostNodeGpuAddress;
    outHostNodeGraphicsAllocation = this->hostNodeAllocation;
    outDeviceGpuAddress = this->deviceNodeGpuAddress + this->offset;
    outDeviceNodeGraphicsAllocation = this->deviceNodeAllocation;
}

} // namespace L0
