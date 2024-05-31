/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"
#include "shared/source/utilities/buffer_pool_allocator.h"

#include <mutex>

namespace NEO {
class GraphicsAllocation;
class Device;

class SharedIsaAllocation {
  public:
    SharedIsaAllocation(GraphicsAllocation *graphicsAllocation, size_t offset, size_t size, std::mutex *mtx)
        : graphicsAllocation(graphicsAllocation), offset(offset), size(size), mtx(*mtx){};

    GraphicsAllocation *getGraphicsAllocation() const {
        return graphicsAllocation;
    }

    size_t getOffset() const {
        return offset;
    }

    size_t getSize() const {
        return size;
    }

    std::unique_lock<std::mutex> obtainSharedAllocationLock() {
        return std::unique_lock<std::mutex>(mtx);
    }

  private:
    GraphicsAllocation *graphicsAllocation;
    const size_t offset;
    const size_t size;
    std::mutex &mtx; // This mutex is shared across all users of this GA
};

// Each shared GA is maintained by single ISAPool
class ISAPool : public AbstractBuffersPool<ISAPool, GraphicsAllocation> {
    using BaseType = AbstractBuffersPool<ISAPool, GraphicsAllocation>;

  public:
    ISAPool(ISAPool &&pool);
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
    SharedIsaAllocation *requestGraphicsAllocationForIsa(bool isBuiltin, size_t size);
    void freeSharedIsaAllocation(SharedIsaAllocation *sharedIsaAllocation);

  private:
    SharedIsaAllocation *tryAllocateISA(bool isBuiltin, size_t size);

    size_t getAllocationSize(bool isBuiltin) const {
        return isBuiltin ? buitinAllocationSize : userAllocationSize;
    }

    Device *device;
    size_t userAllocationSize = MemoryConstants::pageSize2M * 2;
    size_t buitinAllocationSize = MemoryConstants::pageSize64k;
    std::mutex allocatorMtx;
};

} // namespace NEO
