/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/utilities/device_buffers_pool.inl"
#include "shared/source/utilities/generic_pool_allocator.h"
#include "shared/source/utilities/heap_allocator.h"

namespace NEO {

template <PoolTraits Traits>
GenericPool<Traits>::GenericPool(Device *device, size_t poolSize)
    : BaseType(device, nullptr) {
    DEBUG_BREAK_IF(device->getProductHelper().is2MBLocalMemAlignmentEnabled() &&
                   !isAligned(poolSize, MemoryConstants::pageSize2M));

    auto properties = Traits::createAllocationProperties(device, poolSize);
    auto graphicsAllocation = this->memoryManager->allocateGraphicsMemoryWithProperties(properties);
    this->initStorage(graphicsAllocation);
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
                                    this->mtx.get()};
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

    return this->allocateFromPoolsWithGrowth(
        [&] { return allocateFromPools(size); },
        [&] { this->addNewBufferPool(GenericPool<Traits>(device, alignToPoolSize(Traits::defaultPoolSize))); });
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
    : BaseType(device, nullptr) {
    DEBUG_BREAK_IF(device->getProductHelper().is2MBLocalMemAlignmentEnabled() &&
                   !isAligned(poolSize, MemoryConstants::pageSize2M));

    auto properties = Traits::createAllocationProperties(device, poolSize);
    auto graphicsAllocation = this->memoryManager->allocateGraphicsMemoryWithProperties(properties);
    this->initStorage(graphicsAllocation);
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

    return this->allocateFromPoolsWithGrowth(
        [&] { return allocateFromPools(size); },
        [&] { this->addNewBufferPool(GenericViewPool<Traits>(device, alignToPoolSize(getDefaultPoolSize()))); });
}

template <PoolTraits Traits>
void GenericViewPoolAllocator<Traits>::free(GraphicsAllocation *allocation) {
    if (!allocation || !allocation->isView()) {
        return;
    }

    device->getMemoryManager()->removeAllocationFromDownloadAllocationsInCsr(allocation);

    auto memoryOperationsInterface = device->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->memoryOperationsInterface.get();
    if (memoryOperationsInterface) {
        memoryOperationsInterface->free(device, *allocation);
    }

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
    for (auto &pool : this->bufferPools) {
        if (pool.isPoolBuffer(chunk.parent)) {
            pool.chunkAllocator->free(chunk.offset + pool.params.startingOffset, chunk.size);
            return;
        }
    }

    DEBUG_BREAK_IF(true);
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
