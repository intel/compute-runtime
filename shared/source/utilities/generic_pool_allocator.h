/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/utilities/buffer_pool_allocator.h"
#include "shared/source/utilities/pool_allocator_traits.h"
#include "shared/source/utilities/shared_pool_allocation.h"
#include "shared/source/utilities/stackvec.h"

#include <memory>
#include <mutex>
#include <vector>

namespace NEO {

class Device;
class GraphicsAllocation;
class ProductHelper;

struct DeferredFreeContext {
    volatile TagAddressType *tagAddress = nullptr;
    volatile TagAddressType *ucTagAddress = nullptr;
    uint32_t contextId = 0;
    uint32_t tagOffset = 0;
    uint32_t partitionCount = 1;
};

struct DeferredChunk {
    GraphicsAllocation *parent = nullptr;
    GraphicsAllocation *view = nullptr;
    size_t offset = 0;
    size_t size = 0;
    TaskCountType taskCount = 0;
    uint32_t contextId = 0;
};

template <PoolTraits Traits>
class GenericPool : public AbstractBuffersPool<GenericPool<Traits>, GraphicsAllocation> {
    using BaseType = AbstractBuffersPool<GenericPool<Traits>, GraphicsAllocation>;

  public:
    GenericPool(Device *device, size_t poolSize);

    GenericPool(const GenericPool &) = delete;
    GenericPool &operator=(const GenericPool &) = delete;

    GenericPool(GenericPool &&pool) noexcept;
    GenericPool &operator=(GenericPool &&) = delete;

    ~GenericPool() override;

    [[nodiscard]] SharedPoolAllocation *allocate(size_t size);
    const StackVec<GraphicsAllocation *, 1> &getAllocationsVector() const;

  private:
    Device *device = nullptr;
    StackVec<GraphicsAllocation *, 1> poolAllocations;
    std::unique_ptr<std::mutex> mtx;
};

template <PoolTraits Traits>
class GenericPoolAllocator : public AbstractBuffersAllocator<GenericPool<Traits>, GraphicsAllocation> {
  public:
    explicit GenericPoolAllocator(Device *device);

    [[nodiscard]] SharedPoolAllocation *allocate(size_t size);
    void free(SharedPoolAllocation *allocation);

    size_t getDefaultPoolSize() const { return Traits::defaultPoolSize; }

  private:
    SharedPoolAllocation *allocateFromPools(size_t size);
    size_t alignToPoolSize(size_t size) const;

    Device *device = nullptr;
    std::mutex allocatorMtx;
};

template <PoolTraits Traits>
class GenericViewPool : public AbstractBuffersPool<GenericViewPool<Traits>, GraphicsAllocation> {
    using BaseType = AbstractBuffersPool<GenericViewPool<Traits>, GraphicsAllocation>;

  public:
    GenericViewPool(Device *device, size_t poolSize);

    GenericViewPool(const GenericViewPool &) = delete;
    GenericViewPool &operator=(const GenericViewPool &) = delete;

    GenericViewPool(GenericViewPool &&pool) noexcept;
    GenericViewPool &operator=(GenericViewPool &&) = delete;

    ~GenericViewPool() override;

    [[nodiscard]] GraphicsAllocation *allocate(size_t size);
    const StackVec<GraphicsAllocation *, 1> &getAllocationsVector() const;

  private:
    Device *device = nullptr;
    StackVec<GraphicsAllocation *, 1> poolAllocations;
    std::unique_ptr<std::mutex> mtx;
};

template <PoolTraits Traits>
class GenericViewPoolAllocator : public AbstractBuffersAllocator<GenericViewPool<Traits>, GraphicsAllocation> {
  public:
    explicit GenericViewPoolAllocator(Device *device);

    [[nodiscard]] GraphicsAllocation *allocate(size_t size);
    [[nodiscard]] GraphicsAllocation *allocate(size_t size, const DeferredFreeContext *ctx);
    void free(GraphicsAllocation *allocation);
    void free(GraphicsAllocation *allocation, TaskCountType taskCount, uint32_t contextId);

    void processDeferredFrees(const DeferredFreeContext *ctx);
    void releasePools();

    size_t getDefaultPoolSize() const {
        if constexpr (requires { Traits::getDefaultPoolSize(); }) {
            return Traits::getDefaultPoolSize();
        }
        return Traits::defaultPoolSize;
    }

    static bool isEnabled(const ProductHelper &productHelper) {
        if constexpr (requires { Traits::isEnabled(productHelper); }) {
            return Traits::isEnabled(productHelper);
        }
        return true;
    }

  protected:
    GraphicsAllocation *allocateFromPools(size_t size);
    size_t alignToPoolSize(size_t size) const;

    void processDeferredFreesUnlocked(const DeferredFreeContext *ctx);
    bool isChunkReady(const DeferredChunk &chunk, const DeferredFreeContext *ctx) const;
    void returnChunkToPool(const DeferredChunk &chunk);

    Device *device = nullptr;
    std::mutex allocatorMtx;
    std::vector<DeferredChunk> deferredChunks;
};

} // namespace NEO