/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/buffer_pool_allocator.h"
#include "shared/source/utilities/stackvec.h"

#include <mutex>

namespace NEO {
class GraphicsAllocation;
class Device;
class MemoryManager;

class LinearStreamPool : public AbstractBuffersPool<LinearStreamPool, GraphicsAllocation> {
    using BaseType = AbstractBuffersPool<LinearStreamPool, GraphicsAllocation>;

  public:
    LinearStreamPool(Device *device, size_t storageSize);

    LinearStreamPool(const LinearStreamPool &) = delete;
    LinearStreamPool &operator=(const LinearStreamPool &) = delete;

    LinearStreamPool(LinearStreamPool &&pool) noexcept;
    LinearStreamPool &operator=(LinearStreamPool &&other) = delete;

    ~LinearStreamPool() override;

    GraphicsAllocation *allocateLinearStream(size_t requestedSize) const;
    const StackVec<GraphicsAllocation *, 1> &getAllocationsVector();

  private:
    Device *device;
    StackVec<GraphicsAllocation *, 1> stackVec;
    std::unique_ptr<std::mutex> mtx;
};

class LinearStreamPoolAllocator : public AbstractBuffersAllocator<LinearStreamPool, GraphicsAllocation> {
  public:
    explicit LinearStreamPoolAllocator(Device *device);

    GraphicsAllocation *allocateLinearStream(size_t size);
    void freeLinearStream(GraphicsAllocation *allocation);

  private:
    GraphicsAllocation *tryAllocate(size_t size);
    size_t alignToPoolSize(size_t size) const;

    Device *device;
    size_t defaultPoolSize = MemoryConstants::pageSize2M * 2;
    size_t poolAlignment = MemoryConstants::pageSize2M;
    size_t maxRequestedSize = MemoryConstants::pageSize2M;
    std::mutex allocatorMtx;
};

static_assert(NEO::NonCopyable<LinearStreamPool>);

} // namespace NEO
