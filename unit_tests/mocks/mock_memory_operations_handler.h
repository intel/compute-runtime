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
    MemoryOperationsStatus makeResident(GraphicsAllocation &gfxAllocation) { return MemoryOperationsStatus::UNSUPPORTED; }
    MemoryOperationsStatus evict(GraphicsAllocation &gfxAllocation) { return MemoryOperationsStatus::UNSUPPORTED; }
    MemoryOperationsStatus isResident(GraphicsAllocation &gfxAllocation) { return MemoryOperationsStatus::UNSUPPORTED; }
};
} // namespace NEO