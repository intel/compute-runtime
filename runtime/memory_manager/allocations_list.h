/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/memory_manager/graphics_allocation.h"

#include <mutex>

namespace NEO {
class CommandStreamReceiver;

class AllocationsList : public IDList<GraphicsAllocation, true, true> {
  public:
    std::unique_ptr<GraphicsAllocation> detachAllocation(size_t requiredMinimalSize, CommandStreamReceiver &commandStreamReceiver, GraphicsAllocation::AllocationType allocationType);

  private:
    GraphicsAllocation *detachAllocationImpl(GraphicsAllocation *, void *);
};
} // namespace NEO
