/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/common_types.h"
#include "shared/source/memory_manager/allocations_list.h"

namespace NEO {

class InternalAllocationStorage {
  public:
    MOCKABLE_VIRTUAL ~InternalAllocationStorage() = default;
    InternalAllocationStorage(CommandStreamReceiver &commandStreamReceiver);
    MOCKABLE_VIRTUAL void cleanAllocationList(uint32_t waitTaskCount, uint32_t allocationUsage);
    void storeAllocation(std::unique_ptr<GraphicsAllocation> &&gfxAllocation, uint32_t allocationUsage);
    void storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation> &&gfxAllocation, uint32_t allocationUsage, uint32_t taskCount);
    std::unique_ptr<GraphicsAllocation> obtainReusableAllocation(size_t requiredSize, AllocationType allocationType);
    std::unique_ptr<GraphicsAllocation> obtainTemporaryAllocationWithPtr(size_t requiredSize, const void *requiredPtr, AllocationType allocationType);
    AllocationsList &getTemporaryAllocations() { return temporaryAllocations; }
    AllocationsList &getAllocationsForReuse() { return allocationsForReuse; }
    DeviceBitfield getDeviceBitfield() const;

  protected:
    void freeAllocationsList(uint32_t waitTaskCount, AllocationsList &allocationsList);
    CommandStreamReceiver &commandStreamReceiver;

    AllocationsList temporaryAllocations;
    AllocationsList allocationsForReuse;
};
} // namespace NEO
