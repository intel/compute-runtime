/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/memory_manager/memory_operations_status.h"
#include "core/utilities/arrayref.h"

namespace NEO {

class GraphicsAllocation;

class MemoryOperationsHandler {
  public:
    MemoryOperationsHandler() = default;
    virtual ~MemoryOperationsHandler() = default;

    virtual MemoryOperationsStatus makeResident(ArrayRef<GraphicsAllocation *> gfxAllocations) = 0;
    virtual MemoryOperationsStatus evict(GraphicsAllocation &gfxAllocation) = 0;
    virtual MemoryOperationsStatus isResident(GraphicsAllocation &gfxAllocation) = 0;
};
} // namespace NEO
