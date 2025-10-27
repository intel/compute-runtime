/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/buffer_pool_allocator.inl"
#include "shared/source/utilities/generic_pool_allocator.h"
#include "shared/source/utilities/heap_allocator.h"

namespace NEO {

template <PoolTraits Traits>
GenericPool<Traits>::GenericPool(Device *device, size_t poolSize)
    : BaseType(device->getMemoryManager(), nullptr), device(device) {
    DEBUG_BREAK_IF(device->getProductHelper().is2MBLocalMemAlignmentEnabled() &&
                   !isAligned(poolSize, MemoryConstants::pageSize2M));

    auto properties = Traits::createAllocationProperties(device, poolSize);
    auto graphicsAllocation = this->memoryManager->allocateGraphicsMemoryWithProperties(properties);

    this->chunkAllocator.reset(new NEO::HeapAllocator(this->params.startingOffset,
                                                      graphicsAllocation ? graphicsAllocation->getUnderlyingBufferSize() : 0u,
                                                      MemoryConstants::pageSize,
                                                      0u));
    this->mainStorage.reset(graphicsAllocation);
    this->mtx = std::make_unique<std::mutex>();
    stackVec.push_back(graphicsAllocation);
}

template <PoolTraits Traits>
GenericPool<Traits>::GenericPool(GenericPool &&pool) noexcept : BaseType(std::move(pool)) {
    mtx.reset(pool.mtx.release());
    this->stackVec = std::move(pool.stackVec);
    this->device = pool.device;
}

template <PoolTraits Traits>
GenericPool<Traits>::~GenericPool() {
    if (this->mainStorage) {
        device->getMemoryManager()->freeGraphicsMemory(this->mainStorage.release());
    }
}

template <PoolTraits Traits>
SharedPoolAllocation *GenericPool<Traits>::allocate(size_t size) {
    auto offset = static_cast<size_t>(this->chunkAllocator->allocate(size));
    if (offset == 0) {
        return nullptr;
    }
    return new SharedPoolAllocation{this->mainStorage.get(),
                                    offset - this->params.startingOffset,
                                    size,
                                    mtx.get()};
}

template <PoolTraits Traits>
const StackVec<GraphicsAllocation *, 1> &GenericPool<Traits>::getAllocationsVector() {
    return stackVec;
}

template <PoolTraits Traits>
GenericPoolAllocator<Traits>::GenericPoolAllocator(Device *device) : device(device) {}

template <PoolTraits Traits>
SharedPoolAllocation *GenericPoolAllocator<Traits>::requestGraphicsAllocation(size_t size) {
    if (size > Traits::maxAllocationSize) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(allocatorMtx);

    if (this->bufferPools.empty()) {
        this->addNewBufferPool(GenericPool<Traits>(device, alignToPoolSize(Traits::defaultPoolSize)));
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

    this->addNewBufferPool(GenericPool<Traits>(device, alignToPoolSize(Traits::defaultPoolSize)));
    return allocateFromPools(size);
}

template <PoolTraits Traits>
void GenericPoolAllocator<Traits>::freeSharedAllocation(SharedPoolAllocation *sharedPoolAllocation) {
    std::unique_lock lock(allocatorMtx);
    this->tryFreeFromPoolBuffer(sharedPoolAllocation->getGraphicsAllocation(),
                                sharedPoolAllocation->getOffset(),
                                sharedPoolAllocation->getSize());
    delete sharedPoolAllocation;
}

template <PoolTraits Traits>
SharedPoolAllocation *GenericPoolAllocator<Traits>::allocateFromPools(size_t size) {
    for (auto &pool : this->bufferPools) {
        if (auto allocation = pool.allocate(size)) {
            return allocation;
        }
    }
    return nullptr;
}

template <PoolTraits Traits>
size_t GenericPoolAllocator<Traits>::alignToPoolSize(size_t size) const {
    return alignUp(size, Traits::poolAlignment);
}

} // namespace NEO