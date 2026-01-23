/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/linear_stream_pool_allocator.h"

#include "shared/source/device/device.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/buffer_pool_allocator.inl"

namespace NEO {

LinearStreamPool::LinearStreamPool(Device *device, size_t storageSize)
    : BaseType(device->getMemoryManager(), nullptr), device(device) {
    DEBUG_BREAK_IF(device->getProductHelper().is2MBLocalMemAlignmentEnabled() &&
                   !isAligned(storageSize, MemoryConstants::pageSize2M));

    AllocationProperties allocProperties = {device->getRootDeviceIndex(),
                                            true /* allocateMemory*/,
                                            storageSize,
                                            NEO::AllocationType::linearStream,
                                            (device->getNumGenericSubDevices() > 1u) /* multiOsContextCapable */,
                                            false,
                                            device->getDeviceBitfield()};

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(allocProperties);
    this->chunkAllocator.reset(new NEO::HeapAllocator(params.startingOffset,
                                                      graphicsAllocation ? graphicsAllocation->getUnderlyingBufferSize() : 0u,
                                                      MemoryConstants::pageSize,
                                                      0u));
    this->mainStorage.reset(graphicsAllocation);
    this->mtx = std::make_unique<std::mutex>();
    this->stackVec.push_back(graphicsAllocation);
}

LinearStreamPool::LinearStreamPool(LinearStreamPool &&pool) noexcept : BaseType(std::move(pool)) {
    mtx.reset(pool.mtx.release());
    this->stackVec = std::move(pool.stackVec);
    this->device = pool.device;
}

LinearStreamPool::~LinearStreamPool() {
    if (mainStorage && memoryManager) {
        memoryManager->freeGraphicsMemory(mainStorage.release());
    }
}

GraphicsAllocation *LinearStreamPool::allocateLinearStream(size_t requestedSize) const {
    if (!mainStorage) {
        return nullptr;
    }

    auto offset = static_cast<size_t>(this->chunkAllocator->allocate(requestedSize));
    if (offset == 0) {
        return nullptr;
    }

    return this->mainStorage->createView(offset - params.startingOffset, requestedSize);
}

const StackVec<NEO::GraphicsAllocation *, 1> &LinearStreamPool::getAllocationsVector() {
    return stackVec;
}

LinearStreamPoolAllocator::LinearStreamPoolAllocator(Device *device) : device(device) {}

GraphicsAllocation *LinearStreamPoolAllocator::allocateLinearStream(size_t size) {
    if (size > maxRequestedSize) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(allocatorMtx);

    if (this->bufferPools.empty()) {
        addNewBufferPool(LinearStreamPool(device, alignToPoolSize(defaultPoolSize)));
    }

    auto allocFromPool = tryAllocate(size);
    if (allocFromPool != nullptr) {
        return allocFromPool;
    }

    this->drain();

    allocFromPool = tryAllocate(size);
    if (allocFromPool != nullptr) {
        return allocFromPool;
    }

    addNewBufferPool(LinearStreamPool(device, alignToPoolSize(defaultPoolSize)));
    return tryAllocate(size);
}

void LinearStreamPoolAllocator::freeLinearStream(GraphicsAllocation *allocation) {
    if (!allocation || !allocation->isView()) {
        return;
    }

    std::unique_lock lock(allocatorMtx);
    tryFreeFromPoolBuffer(allocation->getParentAllocation(), allocation->getOffsetInParent(), allocation->getUnderlyingBufferSize());
    delete allocation;
}

GraphicsAllocation *LinearStreamPoolAllocator::tryAllocate(size_t size) {
    for (auto &pool : this->bufferPools) {
        if (auto allocation = pool.allocateLinearStream(size)) {
            return allocation;
        }
    }
    return nullptr;
}

size_t LinearStreamPoolAllocator::alignToPoolSize(size_t size) const {
    return alignUp(size, poolAlignment);
}

template class AbstractBuffersAllocator<LinearStreamPool, GraphicsAllocation>;

} // namespace NEO
