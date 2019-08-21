/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/memory_manager/memory_operations_handler.h"

namespace NEO {

class GraphicsAllocation;

class MockMemoryOperationsHandler : public MemoryOperationsHandler {
  public:
    MockMemoryOperationsHandler() {}
    virtual ~MockMemoryOperationsHandler() {}
    bool makeResident(GraphicsAllocation &gfxAllocation) { return false; }
    bool evict(GraphicsAllocation &gfxAllocation) { return false; }
    bool isResident(GraphicsAllocation &gfxAllocation) { return false; }
};
} // namespace NEO