/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"

#include <mutex>

namespace NEO {
class CommandStreamReceiver;
struct ReusableAllocationRequirements;

class AllocationsList : public IDList<GraphicsAllocation, true, true> {
  public:
    AllocationsList(AllocationUsage allocationUsage);
    AllocationsList();
    std::unique_ptr<GraphicsAllocation> detachAllocation(size_t requiredMinimalSize, const void *requiredPtr, CommandStreamReceiver *commandStreamReceiver, AllocationType allocationType);
    void freeAllGraphicsAllocations(Device *neoDevice);

  private:
    GraphicsAllocation *detachAllocationImpl(GraphicsAllocation *, void *);
    bool checkTagAddressReady(ReusableAllocationRequirements *requirements, GraphicsAllocation *gfxAllocation);

    const AllocationUsage allocationUsage;
};
} // namespace NEO
