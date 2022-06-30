/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/utilities/spinlock.h"

#include "third_party/aub_stream/headers/aub_manager.h"

#include <mutex>
#include <vector>

namespace NEO {

class AubMemoryOperationsHandler : public MemoryOperationsHandler {
  public:
    AubMemoryOperationsHandler(aub_stream::AubManager *aubManager);
    ~AubMemoryOperationsHandler() override = default;

    MemoryOperationsStatus makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) override;
    MemoryOperationsStatus evict(Device *device, GraphicsAllocation &gfxAllocation) override;
    MemoryOperationsStatus isResident(Device *device, GraphicsAllocation &gfxAllocation) override;

    MemoryOperationsStatus makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable) override;
    MemoryOperationsStatus evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) override;

    void setAubManager(aub_stream::AubManager *aubManager);

  protected:
    MOCKABLE_VIRTUAL std::unique_lock<SpinLock> acquireLock(SpinLock &lock) {
        return std::unique_lock<SpinLock>{lock};
    }
    aub_stream::AubManager *aubManager = nullptr;
    std::vector<GraphicsAllocation *> residentAllocations;
    SpinLock resourcesLock;
};
} // namespace NEO
