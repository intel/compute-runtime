/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <mutex>

namespace NEO {
class GraphicsAllocation;

class SharedPoolAllocation {
  public:
    SharedPoolAllocation(GraphicsAllocation *graphicsAllocation, size_t offset, size_t size, std::mutex *mtx)
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

} //  namespace NEO