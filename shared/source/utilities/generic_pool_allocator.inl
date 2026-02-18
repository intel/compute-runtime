/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/graphics_allocation.h"
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
    poolAllocations.push_back(graphicsAllocation);
}

template <PoolTraits Traits>
GenericPool<Traits>::GenericPool(GenericPool &&pool) noexcept : BaseType(std::move(pool)) {
    mtx.reset(pool.mtx.release());
    this->poolAllocations = std::move(pool.poolAllocations);
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
const StackVec<GraphicsAllocation *, 1> &GenericPool<Traits>::getAllocationsVector() const {
    return poolAllocations;
}

template <PoolTraits Traits>
GenericPoolAllocator<Traits>::GenericPoolAllocator(Device *device) : device(device) {}

template <PoolTraits Traits>
SharedPoolAllocation *GenericPoolAllocator<Traits>::allocate(size_t size) {
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
void GenericPoolAllocator<Traits>::free(SharedPoolAllocation *allocation) {
    std::unique_lock lock(allocatorMtx);
    this->tryFreeFromPoolBuffer(allocation->getGraphicsAllocation(),
                                allocation->getOffset(),
                                allocation->getSize());
    delete allocation;
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

template <PoolTraits Traits>
GenericViewPool<Traits>::GenericViewPool(Device *device, size_t poolSize)
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
    poolAllocations.push_back(graphicsAllocation);
}

template <PoolTraits Traits>
GenericViewPool<Traits>::GenericViewPool(GenericViewPool &&pool) noexcept : BaseType(std::move(pool)) {
    mtx.reset(pool.mtx.release());
    this->poolAllocations = std::move(pool.poolAllocations);
    this->device = pool.device;
}

template <PoolTraits Traits>
GenericViewPool<Traits>::~GenericViewPool() {
    if (this->mainStorage) {
        device->getMemoryManager()->freeGraphicsMemory(this->mainStorage.release());
    }
}

template <PoolTraits Traits>
GraphicsAllocation *GenericViewPool<Traits>::allocate(size_t size) {
    if (!this->mainStorage) {
        return nullptr;
    }

    auto offset = static_cast<size_t>(this->chunkAllocator->allocate(size));
    if (offset == 0) {
        return nullptr;
    }

    return this->mainStorage->createView(offset - this->params.startingOffset, size);
}

template <PoolTraits Traits>
const StackVec<GraphicsAllocation *, 1> &GenericViewPool<Traits>::getAllocationsVector() const {
    return poolAllocations;
}

template <PoolTraits Traits>
GenericViewPoolAllocator<Traits>::GenericViewPoolAllocator(Device *device) : device(device) {}

template <PoolTraits Traits>
GraphicsAllocation *GenericViewPoolAllocator<Traits>::allocate(size_t size) {
    return allocate(size, nullptr);
}

template <PoolTraits Traits>
GraphicsAllocation *GenericViewPoolAllocator<Traits>::allocate(size_t size, const DeferredFreeContext *ctx) {
    if (size > Traits::maxAllocationSize) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(allocatorMtx);

    if (ctx) {
        processDeferredFreesUnlocked(ctx);
    }

    if (this->bufferPools.empty()) {
        this->addNewBufferPool(GenericViewPool<Traits>(device, alignToPoolSize(getDefaultPoolSize())));
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

    this->addNewBufferPool(GenericViewPool<Traits>(device, alignToPoolSize(getDefaultPoolSize())));
    return allocateFromPools(size);
}

template <PoolTraits Traits>
void GenericViewPoolAllocator<Traits>::free(GraphicsAllocation *allocation) {
    if (!allocation || !allocation->isView()) {
        return;
    }

    device->getMemoryManager()->removeAllocationFromDownloadAllocationsInCsr(allocation);

    std::unique_lock lock(allocatorMtx);
    DEBUG_BREAK_IF(!this->isPoolBuffer(allocation->getParentAllocation()));
    this->tryFreeFromPoolBuffer(allocation->getParentAllocation(),
                                allocation->getOffsetInParent(),
                                allocation->getUnderlyingBufferSize());
    delete allocation;
}

template <PoolTraits Traits>
void GenericViewPoolAllocator<Traits>::free(GraphicsAllocation *allocation, TaskCountType taskCount, uint32_t contextId) {
    if (!allocation || !allocation->isView()) {
        return;
    }

    device->getMemoryManager()->removeAllocationFromDownloadAllocationsInCsr(allocation);

    DeferredChunk chunk{};
    chunk.parent = allocation->getParentAllocation();
    chunk.view = allocation;
    chunk.offset = allocation->getOffsetInParent();
    chunk.size = allocation->getUnderlyingBufferSize();
    chunk.taskCount = taskCount;
    chunk.contextId = contextId;

    std::unique_lock lock(allocatorMtx);
    DEBUG_BREAK_IF(!this->isPoolBuffer(chunk.parent));
    this->deferredChunks.push_back(chunk);
}

template <PoolTraits Traits>
void GenericViewPoolAllocator<Traits>::processDeferredFrees(const DeferredFreeContext *ctx) {
    std::lock_guard<std::mutex> lock(allocatorMtx);
    processDeferredFreesUnlocked(ctx);
}

template <PoolTraits Traits>
void GenericViewPoolAllocator<Traits>::processDeferredFreesUnlocked(const DeferredFreeContext *ctx) {
    if (!ctx || this->deferredChunks.empty()) {
        return;
    }

    std::erase_if(this->deferredChunks, [this, ctx](const DeferredChunk &chunk) {
        if (chunk.contextId == ctx->contextId && isChunkReady(chunk, ctx)) {
            returnChunkToPool(chunk);
            delete chunk.view;
            return true;
        }
        return false;
    });
}

template <PoolTraits Traits>
void GenericViewPoolAllocator<Traits>::releasePools() {
    std::lock_guard<std::mutex> lock(allocatorMtx);

    auto currentMM = this->device->getMemoryManager();
    for (auto &pool : this->bufferPools) {
        pool.memoryManager = currentMM;
    }

    for (const auto &chunk : this->deferredChunks) {
        returnChunkToPool(chunk);
        delete chunk.view;
    }
    this->deferredChunks.clear();

    this->drain();
    AbstractBuffersAllocator<GenericViewPool<Traits>, GraphicsAllocation>::releasePools();
}

template <PoolTraits Traits>
bool GenericViewPoolAllocator<Traits>::isChunkReady(const DeferredChunk &chunk, const DeferredFreeContext *ctx) const {
    if (!ctx || !ctx->tagAddress) {
        return false;
    }

    auto checkTagAddress = [&chunk, ctx](volatile TagAddressType *tagAddr) -> bool {
        for (uint32_t i = 0; i < ctx->partitionCount; i++) {
            if (*tagAddr < chunk.taskCount) {
                return false;
            }
            tagAddr = ptrOffset(tagAddr, ctx->tagOffset);
        }
        return true;
    };

    if (ctx->ucTagAddress && checkTagAddress(ctx->ucTagAddress)) {
        return true;
    }

    return checkTagAddress(ctx->tagAddress);
}

template <PoolTraits Traits>
void GenericViewPoolAllocator<Traits>::returnChunkToPool(const DeferredChunk &chunk) {
    this->tryFreeFromPoolBuffer(chunk.parent, chunk.offset, chunk.size);
}

template <PoolTraits Traits>
GraphicsAllocation *GenericViewPoolAllocator<Traits>::allocateFromPools(size_t size) {
    for (auto &pool : this->bufferPools) {
        if (auto allocation = pool.allocate(size)) {
            return allocation;
        }
    }
    return nullptr;
}

template <PoolTraits Traits>
size_t GenericViewPoolAllocator<Traits>::alignToPoolSize(size_t size) const {
    return alignUp(size, Traits::poolAlignment);
}

} // namespace NEO