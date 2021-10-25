/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"

#include <unordered_set>

namespace NEO {
class OsContextLinux;
class DrmMemoryOperationsHandlerDefault : public DrmMemoryOperationsHandler {
  public:
    DrmMemoryOperationsHandlerDefault();
    ~DrmMemoryOperationsHandlerDefault() override;

    MemoryOperationsStatus makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable) override;
    MemoryOperationsStatus makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) override;
    MemoryOperationsStatus isResident(Device *device, GraphicsAllocation &gfxAllocation) override;
    MemoryOperationsStatus evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) override;
    MemoryOperationsStatus evict(Device *device, GraphicsAllocation &gfxAllocation) override;

    void mergeWithResidencyContainer(OsContext *osContext, ResidencyContainer &residencyContainer) override;
    std::unique_lock<std::mutex> lockHandlerIfUsed() override;

    void evictUnusedAllocations(bool waitForCompletion, bool isLockNeeded) override;

  protected:
    std::unordered_set<GraphicsAllocation *> residency;
};
} // namespace NEO
