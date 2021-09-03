/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/os_interface/os_context.h"

#include "gmock/gmock.h"

namespace NEO {

class GraphicsAllocation;

class MockMemoryOperationsHandler : public MemoryOperationsHandler {
  public:
    MockMemoryOperationsHandler() {}
    MemoryOperationsStatus makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) override { return MemoryOperationsStatus::UNSUPPORTED; }
    MemoryOperationsStatus evict(Device *device, GraphicsAllocation &gfxAllocation) override { return MemoryOperationsStatus::UNSUPPORTED; }
    MemoryOperationsStatus isResident(Device *device, GraphicsAllocation &gfxAllocation) override { return MemoryOperationsStatus::UNSUPPORTED; }
    MemoryOperationsStatus makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable) override { return MemoryOperationsStatus::UNSUPPORTED; }
    MemoryOperationsStatus evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) override { return MemoryOperationsStatus::UNSUPPORTED; }
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
    MOCK_METHOD(MemoryOperationsStatus, makeResidentWithinOsContext,
                (OsContext * osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable), (override));
    MOCK_METHOD(MemoryOperationsStatus, evictWithinOsContext,
                (OsContext * osContext, GraphicsAllocation &gfxAllocation), (override));
};

class MockMemoryOperations : public MemoryOperationsHandler {
  public:
    MockMemoryOperations() {}

    MemoryOperationsStatus makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) override {
        makeResidentCalledCount++;
        return MemoryOperationsStatus::SUCCESS;
    }
    MemoryOperationsStatus evict(Device *device, GraphicsAllocation &gfxAllocation) override {
        evictCalledCount++;
        return MemoryOperationsStatus::SUCCESS;
    }
    MemoryOperationsStatus isResident(Device *device, GraphicsAllocation &gfxAllocation) override {
        return MemoryOperationsStatus::SUCCESS;
    }

    MemoryOperationsStatus makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable) override {
        makeResidentCalledCount++;
        if (osContext) {
            makeResidentContextId = osContext->getContextId();
        }
        return MemoryOperationsStatus::SUCCESS;
    }
    MemoryOperationsStatus evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) override {
        evictCalledCount++;
        return MemoryOperationsStatus::SUCCESS;
    }

    int makeResidentCalledCount = 0;
    int evictCalledCount = 0;
    uint32_t makeResidentContextId = std::numeric_limits<uint32_t>::max();
};

} // namespace NEO
