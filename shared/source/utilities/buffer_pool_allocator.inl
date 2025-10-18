/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/utilities/buffer_pool_allocator.h"
#include "shared/source/utilities/heap_allocator.h"

#include <type_traits>

namespace NEO {

inline SmallBuffersParams SmallBuffersParams::getPreferredBufferPoolParams(const ProductHelper &productHelper) {
    return productHelper.is2MBLocalMemAlignmentEnabled() ? SmallBuffersParams::getLargePagesParams() : SmallBuffersParams::getDefaultParams();
}

template <typename PoolT, typename BufferType, typename BufferParentType>
AbstractBuffersPool<PoolT, BufferType, BufferParentType>::AbstractBuffersPool(MemoryManager *memoryManager, OnChunkFreeCallback onChunkFreeCb)
    : AbstractBuffersPool<PoolT, BufferType, BufferParentType>::AbstractBuffersPool(memoryManager, onChunkFreeCb, SmallBuffersParams::getDefaultParams()) {}

template <typename PoolT, typename BufferType, typename BufferParentType>
AbstractBuffersPool<PoolT, BufferType, BufferParentType>::AbstractBuffersPool(MemoryManager *memoryManager, OnChunkFreeCallback onChunkFreeCb, const SmallBuffersParams &params)
    : memoryManager{memoryManager}, onChunkFreeCallback{onChunkFreeCb}, params{params} {
    static_assert(std::is_base_of_v<BufferParentType, BufferType>);
}

template <typename PoolT, typename BufferType, typename BufferParentType>
AbstractBuffersPool<PoolT, BufferType, BufferParentType>::AbstractBuffersPool(AbstractBuffersPool<PoolT, BufferType, BufferParentType> &&bufferPool) noexcept
    : memoryManager{bufferPool.memoryManager},
      mainStorage{std::move(bufferPool.mainStorage)},
      chunkAllocator{std::move(bufferPool.chunkAllocator)},
      onChunkFreeCallback{bufferPool.onChunkFreeCallback},
      params{bufferPool.params} {}

template <typename PoolT, typename BufferType, typename BufferParentType>
void AbstractBuffersPool<PoolT, BufferType, BufferParentType>::tryFreeFromPoolBuffer(BufferParentType *possiblePoolBuffer, size_t offset, size_t size) {
    if (this->isPoolBuffer(possiblePoolBuffer)) {
        this->chunksToFree.push_back({offset, size});
    }
}

template <typename PoolT, typename BufferType, typename BufferParentType>
bool AbstractBuffersPool<PoolT, BufferType, BufferParentType>::isPoolBuffer(const BufferParentType *buffer) const {
    static_assert(std::is_base_of_v<BufferParentType, BufferType>);

    return (buffer && this->mainStorage.get() == buffer);
}

template <typename PoolT, typename BufferType, typename BufferParentType>
void AbstractBuffersPool<PoolT, BufferType, BufferParentType>::drain() {
    const auto &allocationsVec = this->getAllocationsVector();
    for (auto allocation : allocationsVec) {
        if (allocation && this->memoryManager->allocInUse(*allocation)) {
            return;
        }
    }
    for (auto &chunk : this->chunksToFree) {
        this->chunkAllocator->free(chunk.first + params.startingOffset, chunk.second);
        if (static_cast<PoolT *>(this)->onChunkFreeCallback) {
            (static_cast<PoolT *>(this)->*onChunkFreeCallback)(chunk.first, chunk.second);
        }
    }
    this->chunksToFree.clear();
}

template <typename BuffersPoolType, typename BufferType, typename BufferParentType>
AbstractBuffersAllocator<BuffersPoolType, BufferType, BufferParentType>::AbstractBuffersAllocator(const SmallBuffersParams &params)
    : params{params} {
    DEBUG_BREAK_IF(params.aggregatedSmallBuffersPoolSize < params.smallBufferThreshold);
}

template <typename BuffersPoolType, typename BufferType, typename BufferParentType>
AbstractBuffersAllocator<BuffersPoolType, BufferType, BufferParentType>::AbstractBuffersAllocator()
    : AbstractBuffersAllocator(SmallBuffersParams::getDefaultParams()) {}

template <typename BuffersPoolType, typename BufferType, typename BufferParentType>
bool AbstractBuffersAllocator<BuffersPoolType, BufferType, BufferParentType>::isPoolBuffer(const BufferParentType *buffer) const {
    static_assert(std::is_base_of_v<BufferParentType, BufferType>);

    for (auto &bufferPool : this->bufferPools) {
        if (bufferPool.isPoolBuffer(buffer)) {
            return true;
        }
    }
    return false;
}

template <typename BuffersPoolType, typename BufferType, typename BufferParentType>
void AbstractBuffersAllocator<BuffersPoolType, BufferType, BufferParentType>::tryFreeFromPoolBuffer(BufferParentType *possiblePoolBuffer, size_t offset, size_t size) {
    this->tryFreeFromPoolBuffer(possiblePoolBuffer, offset, size, this->bufferPools);
}

template <typename BuffersPoolType, typename BufferType, typename BufferParentType>
void AbstractBuffersAllocator<BuffersPoolType, BufferType, BufferParentType>::tryFreeFromPoolBuffer(BufferParentType *possiblePoolBuffer, size_t offset, size_t size, std::vector<BuffersPoolType> &bufferPoolsVec) {
    auto lock = std::unique_lock<std::mutex>(this->mutex);
    for (auto &bufferPool : bufferPoolsVec) {
        bufferPool.tryFreeFromPoolBuffer(possiblePoolBuffer, offset, size); // NOLINT(clang-analyzer-cplusplus.NewDelete)
    }
}

template <typename BuffersPoolType, typename BufferType, typename BufferParentType>
void AbstractBuffersAllocator<BuffersPoolType, BufferType, BufferParentType>::drain() {
    this->drain(this->bufferPools);
}

template <typename BuffersPoolType, typename BufferType, typename BufferParentType>
void AbstractBuffersAllocator<BuffersPoolType, BufferType, BufferParentType>::drain(std::vector<BuffersPoolType> &bufferPoolsVec) {
    for (auto &bufferPool : bufferPoolsVec) {
        bufferPool.drain();
    }
}

template <typename BuffersPoolType, typename BufferType, typename BufferParentType>
void AbstractBuffersAllocator<BuffersPoolType, BufferType, BufferParentType>::addNewBufferPool(BuffersPoolType &&bufferPool) {
    this->addNewBufferPool(std::move(bufferPool), this->bufferPools);
}

template <typename BuffersPoolType, typename BufferType, typename BufferParentType>
void AbstractBuffersAllocator<BuffersPoolType, BufferType, BufferParentType>::addNewBufferPool(BuffersPoolType &&bufferPool, std::vector<BuffersPoolType> &bufferPoolsVec) {
    if (bufferPool.mainStorage) {
        bufferPoolsVec.push_back(std::move(bufferPool));
    }
}
} // namespace NEO