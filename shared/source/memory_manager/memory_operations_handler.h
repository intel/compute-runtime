/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/memory_operations_status.h"
#include "shared/source/utilities/arrayref.h"

#include <vector>

namespace NEO {
class Device;
class GraphicsAllocation;
class OsContext;

class MemoryOperationsHandler {
  public:
    MemoryOperationsHandler() = default;
    virtual ~MemoryOperationsHandler() = default;

    virtual MemoryOperationsStatus makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) = 0;
    virtual MemoryOperationsStatus evict(Device *device, GraphicsAllocation &gfxAllocation) = 0;
    virtual MemoryOperationsStatus isResident(Device *device, GraphicsAllocation &gfxAllocation) = 0;

    virtual MemoryOperationsStatus makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable) = 0;
    virtual MemoryOperationsStatus evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) = 0;
};
} // namespace NEO
