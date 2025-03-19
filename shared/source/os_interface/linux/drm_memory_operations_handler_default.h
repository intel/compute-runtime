/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"

#include <vector>

namespace NEO {
class OsContextLinux;
struct RootDeviceEnvironment;
class DrmMemoryOperationsHandlerDefault : public DrmMemoryOperationsHandler {
  public:
    DrmMemoryOperationsHandlerDefault(uint32_t rootDeviceIndex);
    DrmMemoryOperationsHandlerDefault(const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t rootDeviceIndex) : DrmMemoryOperationsHandlerDefault(rootDeviceIndex) {}
    ~DrmMemoryOperationsHandlerDefault() override;

    MemoryOperationsStatus makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable, const bool forcePagingFence, const bool acquireLock) override;
    MemoryOperationsStatus makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations, bool isDummyExecNeeded, const bool forcePagingFence) override;
    MemoryOperationsStatus lock(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) override;
    MemoryOperationsStatus isResident(Device *device, GraphicsAllocation &gfxAllocation) override;
    MemoryOperationsStatus evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) override;
    MemoryOperationsStatus evict(Device *device, GraphicsAllocation &gfxAllocation) override;

    MemoryOperationsStatus mergeWithResidencyContainer(OsContext *osContext, ResidencyContainer &residencyContainer) override;
    [[nodiscard]] std::unique_lock<std::mutex> lockHandlerIfUsed() override;

    MemoryOperationsStatus evictUnusedAllocations(bool waitForCompletion, bool isLockNeeded) override;
    MOCKABLE_VIRTUAL MemoryOperationsStatus flushDummyExec(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations);

    bool obtainAndResetNewResourcesSinceLastRingSubmit() override;

  protected:
    std::vector<GraphicsAllocation *> residency{};
    bool newResourcesSinceLastRingSubmit = false;
};
} // namespace NEO
