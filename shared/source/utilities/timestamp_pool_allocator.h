/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/buffer_pool_allocator.h"
#include "shared/source/utilities/shared_pool_allocation.h"
#include "shared/source/utilities/stackvec.h"

#include <mutex>

namespace NEO {
class GraphicsAllocation;
class Device;

using SharedTimestampAllocation = SharedPoolAllocation;

class TimestampPool : public AbstractBuffersPool<TimestampPool, GraphicsAllocation> {
    using BaseType = AbstractBuffersPool<TimestampPool, GraphicsAllocation>;

  public:
    TimestampPool(Device *device, size_t poolSize);

    TimestampPool(const TimestampPool &) = delete;
    TimestampPool &operator=(const TimestampPool &) = delete;

    TimestampPool(TimestampPool &&pool) noexcept;
    TimestampPool &operator=(TimestampPool &&) = delete;

    ~TimestampPool() override;

    SharedTimestampAllocation *allocate(size_t size);
    const StackVec<GraphicsAllocation *, 1> &getAllocationsVector();

  private:
    Device *device;
    StackVec<GraphicsAllocation *, 1> stackVec;
    std::unique_ptr<std::mutex> mtx;
};

class TimestampPoolAllocator : public AbstractBuffersAllocator<TimestampPool, GraphicsAllocation> {
  public:
    TimestampPoolAllocator(Device *device);

    bool isEnabled() const;

    SharedTimestampAllocation *requestGraphicsAllocationForTimestamp(size_t size);
    void freeSharedTimestampAllocation(SharedTimestampAllocation *sharedTimestampAllocation);

    size_t getDefaultPoolSize() const { return defaultPoolSize; }

  private:
    SharedTimestampAllocation *allocateFromPools(size_t size);
    size_t alignToPoolSize(size_t size) const;

    const size_t maxAllocationSize = 2 * MemoryConstants::megaByte;
    const size_t defaultPoolSize = 4 * MemoryConstants::megaByte;
    const size_t poolAlignment = MemoryConstants::pageSize2M;

    Device *device;
    std::mutex allocatorMtx;
};

static_assert(NEO::NonCopyable<TimestampPool>);

} // namespace NEO
