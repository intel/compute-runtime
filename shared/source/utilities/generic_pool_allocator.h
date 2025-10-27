/*
 * Copyright (C) 2025 Intel Corporation
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

    SharedPoolAllocation *allocate(size_t size);
    const StackVec<GraphicsAllocation *, 1> &getAllocationsVector();

  private:
    Device *device;
    StackVec<GraphicsAllocation *, 1> stackVec;
    std::unique_ptr<std::mutex> mtx;
};

template <PoolTraits Traits>
class GenericPoolAllocator : public AbstractBuffersAllocator<GenericPool<Traits>, GraphicsAllocation> {
  public:
    explicit GenericPoolAllocator(Device *device);

    SharedPoolAllocation *requestGraphicsAllocation(size_t size);
    void freeSharedAllocation(SharedPoolAllocation *sharedPoolAllocation);

    size_t getDefaultPoolSize() const { return Traits::defaultPoolSize; }

  private:
    SharedPoolAllocation *allocateFromPools(size_t size);
    size_t alignToPoolSize(size_t size) const;

    Device *device;
    std::mutex allocatorMtx;
};

} // namespace NEO