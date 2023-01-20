/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/idlist.h"

#include <memory>

namespace NEO {
class CommandStreamReceiver;

class AllocationsList : public IDList<GraphicsAllocation, true, true> {
  public:
    AllocationsList() = default;
    AllocationsList(AllocationUsage allocationUsage);

    std::unique_ptr<GraphicsAllocation> detachAllocation(size_t requiredMinimalSize, const void *requiredPtr, CommandStreamReceiver *commandStreamReceiver, AllocationType allocationType);
    void freeAllGraphicsAllocations(Device *neoDevice);

  private:
    GraphicsAllocation *detachAllocationImpl(GraphicsAllocation *, void *);

    const AllocationUsage allocationUsage{REUSABLE_ALLOCATION};
};
} // namespace NEO
