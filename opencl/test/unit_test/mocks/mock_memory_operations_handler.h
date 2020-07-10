/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/memory_operations_handler.h"

#include "gmock/gmock.h"

namespace NEO {

class GraphicsAllocation;

class MockMemoryOperationsHandler : public MemoryOperationsHandler {
  public:
    MockMemoryOperationsHandler() {}
    MemoryOperationsStatus makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) override { return MemoryOperationsStatus::UNSUPPORTED; }
    MemoryOperationsStatus evict(Device *device, GraphicsAllocation &gfxAllocation) override { return MemoryOperationsStatus::UNSUPPORTED; }
    MemoryOperationsStatus isResident(Device *device, GraphicsAllocation &gfxAllocation) override { return MemoryOperationsStatus::UNSUPPORTED; }
};

class MockMemoryOperationsHandlerTests : public MemoryOperationsHandler {
  public:
    MockMemoryOperationsHandlerTests() {}
    MOCK_METHOD(MemoryOperationsStatus, makeResident,
                (Device * device, ArrayRef<GraphicsAllocation *> gfxAllocations), (override));
    MOCK_METHOD(MemoryOperationsStatus, evict,
                (Device * device, GraphicsAllocation &gfxAllocation), (override));
    MOCK_METHOD(MemoryOperationsStatus, isResident,
                (Device * device, GraphicsAllocation &gfxAllocation), (override));
};

} // namespace NEO
