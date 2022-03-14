/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include "gtest/gtest.h"

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
    ADDMETHOD_NOBASE(makeResident, MemoryOperationsStatus, MemoryOperationsStatus::UNSUPPORTED, (Device * device, ArrayRef<GraphicsAllocation *> gfxAllocations));
    ADDMETHOD_NOBASE(evict, MemoryOperationsStatus, MemoryOperationsStatus::UNSUPPORTED, (Device * device, GraphicsAllocation &gfxAllocation));
    ADDMETHOD_NOBASE(isResident, MemoryOperationsStatus, MemoryOperationsStatus::UNSUPPORTED, (Device * device, GraphicsAllocation &gfxAllocation));
    ADDMETHOD_NOBASE(makeResidentWithinOsContext, MemoryOperationsStatus, MemoryOperationsStatus::UNSUPPORTED, (OsContext * osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable));
    ADDMETHOD_NOBASE(evictWithinOsContext, MemoryOperationsStatus, MemoryOperationsStatus::UNSUPPORTED, (OsContext * osContext, GraphicsAllocation &gfxAllocation));
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
