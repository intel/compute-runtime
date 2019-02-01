/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/memory_manager/graphics_allocation.h"

namespace OCLRT {
class CommandStreamReceiver;

class AllocationsList : public IDList<GraphicsAllocation, true, true> {
  public:
    std::unique_ptr<GraphicsAllocation> detachAllocation(size_t requiredMinimalSize, CommandStreamReceiver &commandStreamReceiver, GraphicsAllocation::AllocationType allocationType);

  private:
    GraphicsAllocation *detachAllocationImpl(GraphicsAllocation *, void *);
};
} // namespace OCLRT
