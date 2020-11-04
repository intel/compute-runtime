/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/graphics_allocation.h"

#include <mutex>

namespace NEO {

class MultiGraphicsAllocation {
  public:
    MultiGraphicsAllocation(uint32_t maxRootDeviceIndex);

    MultiGraphicsAllocation(const MultiGraphicsAllocation &obj);

    GraphicsAllocation *getDefaultGraphicsAllocation() const;

    void addAllocation(GraphicsAllocation *graphicsAllocation);

    void removeAllocation(uint32_t rootDeviceIndex);

    GraphicsAllocation *getGraphicsAllocation(uint32_t rootDeviceIndex) const;

    GraphicsAllocation::AllocationType getAllocationType() const;

    bool isCoherent() const;

    StackVec<GraphicsAllocation *, 1> const &getGraphicsAllocations() const;

    void ensureMemoryOnDevice(MemoryManager &memoryManager, uint32_t requiredRootDeviceIndex);

    uint32_t getLastUsedRootDeviceIndex() const { return lastUsedRootDeviceIndex; }

  protected:
    StackVec<GraphicsAllocation *, 1> graphicsAllocations;

    uint32_t lastUsedRootDeviceIndex = std::numeric_limits<uint32_t>::max();
    uint32_t requiredRootDeviceIndex = std::numeric_limits<uint32_t>::max();

    std::mutex transferMutex;
};

} // namespace NEO
