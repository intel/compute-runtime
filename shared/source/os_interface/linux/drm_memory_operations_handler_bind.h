/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"

namespace NEO {
class DrmMemoryOperationsHandlerBind : public DrmMemoryOperationsHandler {
  public:
    DrmMemoryOperationsHandlerBind();
    ~DrmMemoryOperationsHandlerBind() override;

    MemoryOperationsStatus makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable) override;
    MemoryOperationsStatus makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) override;
    MemoryOperationsStatus evict(Device *device, GraphicsAllocation &gfxAllocation) override;
    MemoryOperationsStatus evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) override;
    MemoryOperationsStatus isResident(Device *device, GraphicsAllocation &gfxAllocation) override;

    void mergeWithResidencyContainer(OsContext *osContext, ResidencyContainer &residencyContainer) override;
    std::unique_lock<std::mutex> lockHandlerForExecWA() override;
};
} // namespace NEO
