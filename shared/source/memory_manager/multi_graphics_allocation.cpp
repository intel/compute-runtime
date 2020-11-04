/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/multi_graphics_allocation.h"

#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/memory_manager.h"

namespace NEO {

MultiGraphicsAllocation::MultiGraphicsAllocation(uint32_t maxRootDeviceIndex) {
    graphicsAllocations.resize(maxRootDeviceIndex + 1);
}

MultiGraphicsAllocation::MultiGraphicsAllocation(const MultiGraphicsAllocation &obj) {
    lastUsedRootDeviceIndex = obj.lastUsedRootDeviceIndex;
    requiredRootDeviceIndex = obj.requiredRootDeviceIndex;
    graphicsAllocations = obj.graphicsAllocations;
}

GraphicsAllocation *MultiGraphicsAllocation::getDefaultGraphicsAllocation() const {
    for (auto &allocation : graphicsAllocations) {
        if (allocation) {
            return allocation;
        }
    }
    return nullptr;
}

void MultiGraphicsAllocation::addAllocation(GraphicsAllocation *graphicsAllocation) {
    UNRECOVERABLE_IF(graphicsAllocation == nullptr);
    UNRECOVERABLE_IF(graphicsAllocations.size() < graphicsAllocation->getRootDeviceIndex() + 1);
    graphicsAllocations[graphicsAllocation->getRootDeviceIndex()] = graphicsAllocation;
}

void MultiGraphicsAllocation::removeAllocation(uint32_t rootDeviceIndex) {
    graphicsAllocations[rootDeviceIndex] = nullptr;
}

GraphicsAllocation *MultiGraphicsAllocation::getGraphicsAllocation(uint32_t rootDeviceIndex) const {
    return graphicsAllocations[rootDeviceIndex];
}

GraphicsAllocation::AllocationType MultiGraphicsAllocation::getAllocationType() const {
    return getDefaultGraphicsAllocation()->getAllocationType();
}

bool MultiGraphicsAllocation::isCoherent() const {
    return getDefaultGraphicsAllocation()->isCoherent();
}

StackVec<GraphicsAllocation *, 1> const &MultiGraphicsAllocation::getGraphicsAllocations() const {
    return graphicsAllocations;
}

void MultiGraphicsAllocation::ensureMemoryOnDevice(MemoryManager &memoryManager, uint32_t requiredRootDeviceIndex) {
    std::unique_lock<std::mutex> lock(transferMutex);
    this->requiredRootDeviceIndex = requiredRootDeviceIndex;

    if (lastUsedRootDeviceIndex == std::numeric_limits<uint32_t>::max()) {
        lastUsedRootDeviceIndex = requiredRootDeviceIndex;
        return;
    }

    if (this->requiredRootDeviceIndex == lastUsedRootDeviceIndex) {
        return;
    }

    if (MemoryPool::isSystemMemoryPool(getGraphicsAllocation(requiredRootDeviceIndex)->getMemoryPool())) {
        lastUsedRootDeviceIndex = requiredRootDeviceIndex;
        return;
    }

    auto srcPtr = memoryManager.lockResource(getGraphicsAllocation(lastUsedRootDeviceIndex));
    auto dstPtr = memoryManager.lockResource(getGraphicsAllocation(requiredRootDeviceIndex));

    memcpy_s(dstPtr, getGraphicsAllocation(requiredRootDeviceIndex)->getUnderlyingBufferSize(),
             srcPtr, getGraphicsAllocation(lastUsedRootDeviceIndex)->getUnderlyingBufferSize());

    memoryManager.unlockResource(getGraphicsAllocation(lastUsedRootDeviceIndex));
    memoryManager.unlockResource(getGraphicsAllocation(requiredRootDeviceIndex));

    lastUsedRootDeviceIndex = requiredRootDeviceIndex;
    lock.unlock();
}

} // namespace NEO
