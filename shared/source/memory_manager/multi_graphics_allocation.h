/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {

class MultiGraphicsAllocation : NonCopyableOrMovableClass {
  public:
    MultiGraphicsAllocation(uint32_t maxRootDeviceIndex);

    GraphicsAllocation *getDefaultGraphicsAllocation() const;

    void addAllocation(GraphicsAllocation *graphicsAllocation);

    GraphicsAllocation *getGraphicsAllocation(uint32_t rootDeviceIndex) const;

    GraphicsAllocation::AllocationType getAllocationType() const;

    bool isCoherent() const;

  protected:
    std::vector<GraphicsAllocation *> graphicsAllocations;
};

} // namespace NEO
