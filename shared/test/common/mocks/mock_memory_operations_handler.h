/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include <algorithm>

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
        if (captureGfxAllocationsForMakeResident) {
            for (auto &gfxAllocation : gfxAllocations) {
                if (!gfxAllocation->getAubInfo().writeMemoryOnly) {
                    gfxAllocationsForMakeResident.push_back(gfxAllocation);
                }
            }
        }
        return MemoryOperationsStatus::SUCCESS;
    }
    MemoryOperationsStatus evict(Device *device, GraphicsAllocation &gfxAllocation) override {
        evictCalledCount++;
        if (captureGfxAllocationsForMakeResident) {
            auto allocIterator = std::find(gfxAllocationsForMakeResident.begin(), gfxAllocationsForMakeResident.end(), &gfxAllocation);
            if (allocIterator != gfxAllocationsForMakeResident.end()) {
                gfxAllocationsForMakeResident.erase(allocIterator);
                return MemoryOperationsStatus::SUCCESS;
            } else {
                return MemoryOperationsStatus::MEMORY_NOT_FOUND;
            }
        }
        return MemoryOperationsStatus::SUCCESS;
    }
    MemoryOperationsStatus isResident(Device *device, GraphicsAllocation &gfxAllocation) override {
        isResidentCalledCount++;
        if (captureGfxAllocationsForMakeResident) {
            if (std::find(gfxAllocationsForMakeResident.begin(), gfxAllocationsForMakeResident.end(), &gfxAllocation) != gfxAllocationsForMakeResident.end()) {
                return MemoryOperationsStatus::SUCCESS;
            } else {
                return MemoryOperationsStatus::MEMORY_NOT_FOUND;
            }
        }
        return MemoryOperationsStatus::SUCCESS;
    }

    MemoryOperationsStatus makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable) override {
        makeResidentCalledCount++;
        if (osContext) {
            makeResidentContextId = osContext->getContextId();
        }
        if (captureGfxAllocationsForMakeResident) {
            for (auto &gfxAllocation : gfxAllocations) {
                gfxAllocationsForMakeResident.push_back(gfxAllocation);
            }
        }
        return MemoryOperationsStatus::SUCCESS;
    }
    MemoryOperationsStatus evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) override {
        evictCalledCount++;
        return MemoryOperationsStatus::SUCCESS;
    }

    std::vector<GraphicsAllocation *> gfxAllocationsForMakeResident{};
    int makeResidentCalledCount = 0;
    int evictCalledCount = 0;
    uint32_t isResidentCalledCount = 0;
    uint32_t makeResidentContextId = std::numeric_limits<uint32_t>::max();
    bool captureGfxAllocationsForMakeResident = false;
};

} // namespace NEO
