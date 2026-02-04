/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/buffer_pool_allocator.h"
#include "shared/source/utilities/pool_allocator_traits.h"
#include "shared/source/utilities/shared_pool_allocation.h"
#include "shared/source/utilities/stackvec.h"

#include <memory>
#include <mutex>

namespace NEO {

class Device;
class GraphicsAllocation;

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
    void free(GraphicsAllocation *allocation);

    size_t getDefaultPoolSize() const { return Traits::defaultPoolSize; }

  private:
    GraphicsAllocation *allocateFromPools(size_t size);
    size_t alignToPoolSize(size_t size) const;

    Device *device = nullptr;
    std::mutex allocatorMtx;
};

} // namespace NEO