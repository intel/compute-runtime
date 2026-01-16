/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/command_buffer_pool_allocator.h"

#include "shared/source/device/device.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/buffer_pool_allocator.inl"

namespace NEO {

CommandBufferPool::CommandBufferPool(Device *device, size_t storageSize)
    : BaseType(device->getMemoryManager(), nullptr), device(device) {
    DEBUG_BREAK_IF(device->getProductHelper().is2MBLocalMemAlignmentEnabled() &&
                   !isAligned(storageSize, MemoryConstants::pageSize2M));

    AllocationProperties allocProperties = {device->getRootDeviceIndex(),
                                            true /* allocateMemory*/,
                                            storageSize,
                                            NEO::AllocationType::commandBuffer,
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

CommandBufferPool::CommandBufferPool(CommandBufferPool &&pool) noexcept : BaseType(std::move(pool)) {
    mtx.reset(pool.mtx.release());
    this->stackVec = std::move(pool.stackVec);
    this->device = pool.device;
}

CommandBufferPool::~CommandBufferPool() {
    if (mainStorage) {
        device->getMemoryManager()->freeGraphicsMemory(mainStorage.release());
    }
}

GraphicsAllocation *CommandBufferPool::allocateCommandBuffer(size_t requestedSize) const {
    auto offset = static_cast<size_t>(this->chunkAllocator->allocate(requestedSize));
    if (offset == 0) {
        return nullptr;
    }

    return this->mainStorage->createView(offset - params.startingOffset, requestedSize);
}

const StackVec<NEO::GraphicsAllocation *, 1> &CommandBufferPool::getAllocationsVector() {
    return stackVec;
}

CommandBufferPoolAllocator::CommandBufferPoolAllocator(Device *device) : device(device) {}

GraphicsAllocation *CommandBufferPoolAllocator::allocateCommandBuffer(size_t size) {
    if (size > maxRequestedSize) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(allocatorMtx);

    if (this->bufferPools.empty()) {
        addNewBufferPool(CommandBufferPool(device, alignToPoolSize(defaultPoolSize)));
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

    addNewBufferPool(CommandBufferPool(device, alignToPoolSize(defaultPoolSize)));
    return tryAllocate(size);
}

void CommandBufferPoolAllocator::freeCommandBuffer(GraphicsAllocation *allocation) {
    if (!allocation || !allocation->isView()) {
        return;
    }

    std::unique_lock lock(allocatorMtx);
    tryFreeFromPoolBuffer(allocation->getParentAllocation(), allocation->getOffsetInParent(), allocation->getUnderlyingBufferSize());
    delete allocation;
}

GraphicsAllocation *CommandBufferPoolAllocator::tryAllocate(size_t size) {
    for (auto &pool : this->bufferPools) {
        if (auto allocation = pool.allocateCommandBuffer(size)) {
            return allocation;
        }
    }
    return nullptr;
}

size_t CommandBufferPoolAllocator::alignToPoolSize(size_t size) const {
    return alignUp(size, poolAlignment);
}

} // namespace NEO