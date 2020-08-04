/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {

class MultiGraphicsAllocation {
  public:
    MultiGraphicsAllocation(uint32_t maxRootDeviceIndex);

    GraphicsAllocation *getDefaultGraphicsAllocation() const;

    void addAllocation(GraphicsAllocation *graphicsAllocation);

    void removeAllocation(uint32_t rootDeviceIndex);

    GraphicsAllocation *getGraphicsAllocation(uint32_t rootDeviceIndex) const;

    GraphicsAllocation::AllocationType getAllocationType() const;

    bool isCoherent() const;

    StackVec<GraphicsAllocation *, 1> const &getGraphicsAllocations() const;

  protected:
    StackVec<GraphicsAllocation *, 1> graphicsAllocations;
};

} // namespace NEO
