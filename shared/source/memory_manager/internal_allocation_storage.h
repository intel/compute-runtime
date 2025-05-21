/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/allocations_list.h"

#include <array>

namespace NEO {

class InternalAllocationStorage {
  public:
    MOCKABLE_VIRTUAL ~InternalAllocationStorage() = default;
    InternalAllocationStorage(CommandStreamReceiver &commandStreamReceiver);
    MOCKABLE_VIRTUAL void cleanAllocationList(TaskCountType waitTaskCount, uint32_t allocationUsage);
    void storeAllocation(std::unique_ptr<GraphicsAllocation> &&gfxAllocation, uint32_t allocationUsage);
    void storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation> &&gfxAllocation, uint32_t allocationUsage, TaskCountType taskCount);
    std::unique_ptr<GraphicsAllocation> obtainReusableAllocation(size_t requiredSize, AllocationType allocationType);
    std::unique_ptr<GraphicsAllocation> obtainTemporaryAllocationWithPtr(size_t requiredSize, const void *requiredPtr, AllocationType allocationType);
    AllocationsList &getTemporaryAllocations();
    AllocationsList &getAllocationsForReuse() { return allocationLists[REUSABLE_ALLOCATION]; }
    AllocationsList &getDeferredAllocations() { return allocationLists[DEFERRED_DEALLOCATION]; }
    DeviceBitfield getDeviceBitfield() const;

  protected:
    void freeAllocationsList(TaskCountType waitTaskCount, AllocationsList &allocationsList);
    CommandStreamReceiver &commandStreamReceiver;

    std::array<AllocationsList, 3> allocationLists = {AllocationsList(TEMPORARY_ALLOCATION), AllocationsList(REUSABLE_ALLOCATION), AllocationsList(DEFERRED_DEALLOCATION)};
};
} // namespace NEO
