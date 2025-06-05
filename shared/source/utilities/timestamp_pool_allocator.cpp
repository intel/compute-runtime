/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/timestamp_pool_allocator.h"

#include "shared/source/device/device.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/buffer_pool_allocator.inl"

namespace NEO {
TimestampPool::TimestampPool(Device *device, size_t poolSize)
    : BaseType(device->getMemoryManager(), nullptr), device(device) {
    DEBUG_BREAK_IF(device->getProductHelper().is2MBLocalMemAlignmentEnabled() &&
                   !isAligned(poolSize, MemoryConstants::pageSize2M));

    AllocationProperties properties{device->getRootDeviceIndex(),
                                    poolSize,
                                    AllocationType::gpuTimestampDeviceBuffer,
                                    device->getDeviceBitfield()};
    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    this->mainStorage.reset(graphicsAllocation);
    this->chunkAllocator.reset(new HeapAllocator(params.startingOffset, poolSize, MemoryConstants::pageSize, 0u));
    stackVec.push_back(graphicsAllocation);
    this->mtx = std::make_unique<std::mutex>();
}

TimestampPool::TimestampPool(TimestampPool &&pool) noexcept : BaseType(std::move(pool)) {
    mtx.reset(pool.mtx.release());
    this->stackVec = std::move(pool.stackVec);
    this->device = pool.device;
}

TimestampPool::~TimestampPool() {
    if (mainStorage) {
        device->getMemoryManager()->freeGraphicsMemory(mainStorage.release());
    }
}

SharedTimestampAllocation *TimestampPool::allocate(size_t size) {
    auto offset = static_cast<size_t>(this->chunkAllocator->allocate(size));
    if (offset == 0) {
        return nullptr;
    }
    return new SharedTimestampAllocation{this->mainStorage.get(), offset - params.startingOffset, size, mtx.get()};
}

const StackVec<GraphicsAllocation *, 1> &TimestampPool::getAllocationsVector() {
    return stackVec;
}

TimestampPoolAllocator::TimestampPoolAllocator(Device *device) : device(device) {}

bool TimestampPoolAllocator::isEnabled() const {
    if (NEO::debugManager.flags.EnableTimestampPoolAllocator.get() != -1) {
        return NEO::debugManager.flags.EnableTimestampPoolAllocator.get();
    }

    return false;
}

SharedTimestampAllocation *TimestampPoolAllocator::requestGraphicsAllocationForTimestamp(size_t size) {
    if (size > maxAllocationSize) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(allocatorMtx);

    if (bufferPools.empty()) {
        addNewBufferPool(TimestampPool(device, alignToPoolSize(defaultPoolSize)));
    }

    auto allocFromPool = allocateFromPools(size);
    if (allocFromPool != nullptr) {
        return allocFromPool;
    }

    this->drain();

    allocFromPool = allocateFromPools(size);
    if (allocFromPool != nullptr) {
        return allocFromPool;
    }

    addNewBufferPool(TimestampPool(device, alignToPoolSize(defaultPoolSize)));
    return allocateFromPools(size);
}

void TimestampPoolAllocator::freeSharedTimestampAllocation(SharedTimestampAllocation *sharedTimestampAllocation) {
    std::unique_lock lock(allocatorMtx);
    tryFreeFromPoolBuffer(sharedTimestampAllocation->getGraphicsAllocation(), sharedTimestampAllocation->getOffset(), sharedTimestampAllocation->getSize());
    delete sharedTimestampAllocation;
}

SharedTimestampAllocation *TimestampPoolAllocator::allocateFromPools(size_t size) {
    for (auto &pool : bufferPools) {
        if (auto allocation = pool.allocate(size)) {
            return allocation;
        }
    }
    return nullptr;
}

size_t TimestampPoolAllocator::alignToPoolSize(size_t size) const {
    return alignUp(size, poolAlignment);
}

} // namespace NEO
