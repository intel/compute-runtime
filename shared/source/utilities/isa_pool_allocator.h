/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/buffer_pool_allocator.h"
#include "shared/source/utilities/shared_pool_allocation.h"

#include <mutex>

namespace NEO {
class GraphicsAllocation;
class Device;

using SharedIsaAllocation = SharedPoolAllocation;

// Each shared GA is maintained by single ISAPool
class ISAPool : public AbstractBuffersPool<ISAPool, GraphicsAllocation> {
    using BaseType = AbstractBuffersPool<ISAPool, GraphicsAllocation>;

  public:
    ISAPool(ISAPool &&pool) noexcept;
    ISAPool &operator=(ISAPool &&other) = delete;
    ISAPool(Device *device, bool isBuiltin, size_t storageSize);
    ~ISAPool() override;

    SharedIsaAllocation *allocateISA(size_t requestedSize) const;
    const StackVec<GraphicsAllocation *, 1> &getAllocationsVector();
    bool isBuiltinPool() const { return isBuiltin; }

  private:
    Device *device;
    bool isBuiltin;
    StackVec<GraphicsAllocation *, 1> stackVec;
    std::unique_ptr<std::mutex> mtx;
};

class ISAPoolAllocator : public AbstractBuffersAllocator<ISAPool, GraphicsAllocation> {
  public:
    ISAPoolAllocator(Device *device);
    SharedIsaAllocation *requestGraphicsAllocationForIsa(bool isBuiltin, size_t sizeWithPadding);
    void freeSharedIsaAllocation(SharedIsaAllocation *sharedIsaAllocation);

  private:
    void initAllocParams();
    SharedIsaAllocation *tryAllocateISA(bool isBuiltin, size_t size);

    size_t getAllocationSize(bool isBuiltin) const {
        return isBuiltin ? builtinAllocationSize : userAllocationSize;
    }
    size_t alignToPoolSize(size_t size) const;

    Device *device;
    size_t userAllocationSize = MemoryConstants::pageSize2M * 2;
    size_t builtinAllocationSize = MemoryConstants::pageSize64k;
    size_t poolAlignment = 1u;
    std::mutex allocatorMtx;
};

static_assert(NEO::NonCopyable<ISAPool>);

} // namespace NEO
