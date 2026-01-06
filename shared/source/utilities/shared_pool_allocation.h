/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/graphics_allocation.h"

#include <mutex>

namespace NEO {

class SharedPoolAllocation {
  public:
    SharedPoolAllocation(GraphicsAllocation *graphicsAllocation, size_t offset, size_t size, std::mutex *mtx)
        : SharedPoolAllocation(graphicsAllocation, offset, size, mtx, true){};

    SharedPoolAllocation(GraphicsAllocation *graphicsAllocation, size_t offset, size_t size, std::mutex *mtx, bool isFromPool)
        : graphicsAllocation(graphicsAllocation), offset(offset), size(size), mtx(mtx), fromPool(isFromPool){};

    explicit SharedPoolAllocation(GraphicsAllocation *graphicsAllocation)
        : graphicsAllocation(graphicsAllocation), offset(0u), size(graphicsAllocation ? graphicsAllocation->getUnderlyingBufferSize() : 0u), mtx(nullptr), fromPool(false){};

    GraphicsAllocation *getGraphicsAllocation() const {
        return graphicsAllocation;
    }

    size_t getOffset() const {
        return offset;
    }

    size_t getSize() const {
        return size;
    }

    uint64_t getGpuAddress() const {
        UNRECOVERABLE_IF(graphicsAllocation == nullptr);
        return graphicsAllocation->getGpuAddress() + offset;
    }

    uint64_t getGpuAddressToPatch() const {
        UNRECOVERABLE_IF(graphicsAllocation == nullptr);
        return graphicsAllocation->getGpuAddressToPatch() + offset;
    }

    void *getUnderlyingBuffer() const {
        UNRECOVERABLE_IF(graphicsAllocation == nullptr);
        return ptrOffset(graphicsAllocation->getUnderlyingBuffer(), offset);
    }

    std::unique_lock<std::mutex> obtainSharedAllocationLock() {
        if (mtx) {
            return std::unique_lock<std::mutex>(*mtx);
        } else {
            DEBUG_BREAK_IF(true);
            return std::unique_lock<std::mutex>();
        }
    }

    std::mutex *getMutex() const {
        return mtx;
    }

    bool isFromPool() const {
        return fromPool;
    }

  private:
    GraphicsAllocation *graphicsAllocation;
    const size_t offset;
    const size_t size;
    std::mutex *mtx; // This mutex is shared across all users of this GA
    const bool fromPool = true;
};

} //  namespace NEO