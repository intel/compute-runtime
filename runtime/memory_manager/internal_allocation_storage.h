/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/memory_manager/allocations_list.h"
#include <cstdint>

namespace OCLRT {
class CommandStreamReceiver;
class GraphicsAllocation;

class InternalAllocationStorage {
  public:
    MOCKABLE_VIRTUAL ~InternalAllocationStorage() = default;
    InternalAllocationStorage(CommandStreamReceiver &commandStreamReceiver);
    MOCKABLE_VIRTUAL void cleanAllocationList(uint32_t waitTaskCount, uint32_t allocationUsage);
    void storeAllocation(std::unique_ptr<GraphicsAllocation> gfxAllocation, uint32_t allocationUsage);
    void storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation> gfxAllocation, uint32_t allocationUsage, uint32_t taskCount);
    std::unique_ptr<GraphicsAllocation> obtainReusableAllocation(size_t requiredSize, bool isInternalAllocationRequired);
    AllocationsList &getTemporaryAllocations() { return temporaryAllocations; }
    AllocationsList &getAllocationsForReuse() { return allocationsForReuse; }

  protected:
    void freeAllocationsList(uint32_t waitTaskCount, AllocationsList &allocationsList);
    CommandStreamReceiver &commandStreamReceiver;
    const uint32_t contextId;

    AllocationsList temporaryAllocations;
    AllocationsList allocationsForReuse;
};
} // namespace OCLRT
