/*
 * Copyright (C) 2018-2020 Intel Corporation
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

class AllocationsList : public IDList<GraphicsAllocation, true, true> {
  public:
    AllocationsList(AllocationUsage allocationUsage);
    std::unique_ptr<GraphicsAllocation> detachAllocation(size_t requiredMinimalSize, const void *requiredPtr, CommandStreamReceiver &commandStreamReceiver, GraphicsAllocation::AllocationType allocationType);

  private:
    GraphicsAllocation *detachAllocationImpl(GraphicsAllocation *, void *);

    const AllocationUsage allocationUsage;
};
} // namespace NEO
