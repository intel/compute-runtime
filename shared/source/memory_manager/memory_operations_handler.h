/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/memory_operations_status.h"
#include "shared/source/utilities/arrayref.h"

namespace NEO {
class Device;
class GraphicsAllocation;
class OsContext;
class CommandStreamReceiver;

class MemoryOperationsHandler {
  public:
    MemoryOperationsHandler() = default;
    virtual ~MemoryOperationsHandler() = default;

    virtual MemoryOperationsStatus makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations, bool isDummyExecNeeded, const bool forcePagingFence) = 0;
    virtual MemoryOperationsStatus decompress(Device *device, GraphicsAllocation &gfxAllocation) = 0;
    virtual MemoryOperationsStatus lock(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) = 0;
    virtual MemoryOperationsStatus evict(Device *device, GraphicsAllocation &gfxAllocation) = 0;
    virtual MemoryOperationsStatus isResident(Device *device, GraphicsAllocation &gfxAllocation) = 0;
    virtual MemoryOperationsStatus free(Device *device, GraphicsAllocation &gfxAllocation) { return MemoryOperationsStatus::success; }

    virtual MemoryOperationsStatus makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable, const bool forcePagingFence, const bool acquireLock) = 0;
    virtual MemoryOperationsStatus evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) = 0;
    virtual void processFlushResidency(CommandStreamReceiver *csr) {}

    virtual MemoryOperationsStatus makeResidentAsync(OsContext *osContext, GraphicsAllocation *gfxAllocation) {
        return makeResidentWithinOsContext(osContext, ArrayRef<GraphicsAllocation *>(&gfxAllocation, 1u), false, false, true);
    }
    virtual MemoryOperationsStatus waitForAsyncResidency(OsContext *osContext, GraphicsAllocation *gfxAllocation) {
        return MemoryOperationsStatus::success;
    }
};
} // namespace NEO
