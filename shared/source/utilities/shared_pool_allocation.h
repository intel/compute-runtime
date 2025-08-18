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
        : graphicsAllocation(graphicsAllocation), offset(offset), size(size), mtx(mtx){};

    explicit SharedPoolAllocation(GraphicsAllocation *graphicsAllocation)
        : graphicsAllocation(graphicsAllocation), offset(0u), size(graphicsAllocation ? graphicsAllocation->getUnderlyingBufferSize() : 0u), mtx(nullptr){};

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

  private:
    GraphicsAllocation *graphicsAllocation;
    const size_t offset;
    const size_t size;
    std::mutex *mtx; // This mutex is shared across all users of this GA
};

} //  namespace NEO