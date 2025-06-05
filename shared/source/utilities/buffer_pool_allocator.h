/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/stackvec.h"

#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <vector>

namespace NEO {

class GraphicsAllocation;
class HeapAllocator;
class MemoryManager;
class ProductHelper;

struct SmallBuffersParams {
    size_t aggregatedSmallBuffersPoolSize{0};
    size_t smallBufferThreshold{0};
    size_t chunkAlignment{0};
    size_t startingOffset{0};

    static SmallBuffersParams getDefaultParams() {
        return {
            .aggregatedSmallBuffersPoolSize = 2 * MemoryConstants::megaByte,
            .smallBufferThreshold = 1 * MemoryConstants::megaByte,
            .chunkAlignment = MemoryConstants::pageSize64k,
            .startingOffset = MemoryConstants::pageSize64k};
    }

    static SmallBuffersParams getLargePagesParams() {
        return {
            .aggregatedSmallBuffersPoolSize = 16 * MemoryConstants::megaByte,
            .smallBufferThreshold = 2 * MemoryConstants::megaByte,
            .chunkAlignment = MemoryConstants::pageSize64k,
            .startingOffset = MemoryConstants::pageSize64k};
    }

    static inline SmallBuffersParams getPreferredBufferPoolParams(const ProductHelper &productHelper);
};

template <typename PoolT, typename BufferType, typename BufferParentType = BufferType>
struct AbstractBuffersPool : public NonCopyableClass {
    // The prototype of a function allocating the `mainStorage` is not specified.
    // That would be an unnecessary limitation here - it is completely up to derived class implementation.
    // Perhaps the allocating function needs to leverage `HeapAllocator::allocate()` and also
    // a BufferType-dependent function reserving chunks within `mainStorage`.
    // Example: see `NEO::Context::BufferPool::allocate()`
    using AllocsVecCRef = const StackVec<NEO::GraphicsAllocation *, 1> &;
    using OnChunkFreeCallback = void (PoolT::*)(uint64_t offset, size_t size);

    AbstractBuffersPool(MemoryManager *memoryManager, OnChunkFreeCallback onChunkFreeCallback);
    AbstractBuffersPool(MemoryManager *memoryManager, OnChunkFreeCallback onChunkFreeCallback, const SmallBuffersParams &params);
    AbstractBuffersPool(AbstractBuffersPool<PoolT, BufferType, BufferParentType> &&bufferPool) noexcept;
    AbstractBuffersPool &operator=(AbstractBuffersPool &&other) noexcept = delete;
    virtual ~AbstractBuffersPool() = default;

    void tryFreeFromPoolBuffer(BufferParentType *possiblePoolBuffer, size_t offset, size_t size);
    bool isPoolBuffer(const BufferParentType *buffer) const;
    void drain();

    // Derived class needs to provide its own implementation of getAllocationsVector().
    // This is a CRTP-replacement for virtual functions.
    AllocsVecCRef getAllocationsVector() {
        return static_cast<PoolT *>(this)->getAllocationsVector();
    }

    MemoryManager *memoryManager{nullptr};
    std::unique_ptr<BufferType> mainStorage;
    std::unique_ptr<HeapAllocator> chunkAllocator;
    std::vector<std::pair<uint64_t, size_t>> chunksToFree;
    OnChunkFreeCallback onChunkFreeCallback = nullptr;
    SmallBuffersParams params;
};

template <typename BuffersPoolType, typename BufferType, typename BufferParentType = BufferType>
class AbstractBuffersAllocator {
    // The prototype of a function allocating buffers from the pool is not specified (see similar comment in `AbstractBufersPool`).
    // By common sense, in order to allocate buffers from the pool the function should leverage a call provided by `BuffersPoolType`.
    // Example: see `NEO::Context::BufferPoolAllocator::allocateBufferFromPool()`.
  public:
    AbstractBuffersAllocator(const SmallBuffersParams &params);
    AbstractBuffersAllocator();

    void releasePools() { this->bufferPools.clear(); }
    bool isPoolBuffer(const BufferParentType *buffer) const;
    void tryFreeFromPoolBuffer(BufferParentType *possiblePoolBuffer, size_t offset, size_t size);
    uint32_t getPoolsCount() { return static_cast<uint32_t>(this->bufferPools.size()); }

    void setParams(const SmallBuffersParams &newParams) {
        params = newParams;
    }
    SmallBuffersParams getParams() const {
        return params;
    };

  protected:
    inline bool isSizeWithinThreshold(size_t size) const { return params.smallBufferThreshold >= size; }
    void tryFreeFromPoolBuffer(BufferParentType *possiblePoolBuffer, size_t offset, size_t size, std::vector<BuffersPoolType> &bufferPoolsVec);
    void drain();
    void drain(std::vector<BuffersPoolType> &bufferPoolsVec);
    void addNewBufferPool(BuffersPoolType &&bufferPool);
    void addNewBufferPool(BuffersPoolType &&bufferPool, std::vector<BuffersPoolType> &bufferPoolsVec);

    std::mutex mutex;
    std::vector<BuffersPoolType> bufferPools;
    SmallBuffersParams params;
};
} // namespace NEO
