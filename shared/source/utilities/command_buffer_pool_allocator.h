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

class CommandBufferPool : public AbstractBuffersPool<CommandBufferPool, GraphicsAllocation> {
    using BaseType = AbstractBuffersPool<CommandBufferPool, GraphicsAllocation>;

  public:
    CommandBufferPool(Device *device, size_t storageSize);

    CommandBufferPool(const CommandBufferPool &) = delete;
    CommandBufferPool &operator=(const CommandBufferPool &) = delete;

    CommandBufferPool(CommandBufferPool &&pool) noexcept;
    CommandBufferPool &operator=(CommandBufferPool &&other) = delete;

    ~CommandBufferPool() override;

    GraphicsAllocation *allocateCommandBuffer(size_t requestedSize) const;
    const StackVec<GraphicsAllocation *, 1> &getAllocationsVector();

  private:
    Device *device;
    StackVec<GraphicsAllocation *, 1> stackVec;
    std::unique_ptr<std::mutex> mtx;
};

class CommandBufferPoolAllocator : public AbstractBuffersAllocator<CommandBufferPool, GraphicsAllocation> {
  public:
    explicit CommandBufferPoolAllocator(Device *device);

    GraphicsAllocation *allocateCommandBuffer(size_t size);
    void freeCommandBuffer(GraphicsAllocation *allocation);

  private:
    GraphicsAllocation *tryAllocate(size_t size);
    size_t alignToPoolSize(size_t size) const;

    Device *device;
    size_t defaultPoolSize = MemoryConstants::pageSize2M * 4;
    size_t poolAlignment = MemoryConstants::pageSize2M;
    size_t maxRequestedSize = MemoryConstants::pageSize2M;
    std::mutex allocatorMtx;
};

static_assert(NEO::NonCopyable<CommandBufferPool>);

} // namespace NEO