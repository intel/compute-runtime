/*
 * Copyright (C) 2019-2025 Intel Corporation
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
    MemoryOperationsStatus makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations, bool isDummyExecNeeded, const bool forcePagingFence) override { return MemoryOperationsStatus::unsupported; }
    MemoryOperationsStatus lock(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) override { return MemoryOperationsStatus::unsupported; }
    MemoryOperationsStatus evict(Device *device, GraphicsAllocation &gfxAllocation) override { return MemoryOperationsStatus::unsupported; }
    MemoryOperationsStatus isResident(Device *device, GraphicsAllocation &gfxAllocation) override { return MemoryOperationsStatus::unsupported; }
    MemoryOperationsStatus makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable, const bool forcePagingFence, const bool acquireLock) override { return MemoryOperationsStatus::unsupported; }
    MemoryOperationsStatus evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) override { return MemoryOperationsStatus::unsupported; }
};

class MockMemoryOperationsHandlerTests : public MemoryOperationsHandler {
  public:
    MockMemoryOperationsHandlerTests() {}
    ADDMETHOD_NOBASE(makeResident, MemoryOperationsStatus, MemoryOperationsStatus::unsupported, (Device * device, ArrayRef<GraphicsAllocation *> gfxAllocations, bool isDummyExecNeeded, const bool forcePagingFence));
    ADDMETHOD_NOBASE(lock, MemoryOperationsStatus, MemoryOperationsStatus::unsupported, (Device * device, ArrayRef<GraphicsAllocation *> gfxAllocations));
    ADDMETHOD_NOBASE(evict, MemoryOperationsStatus, MemoryOperationsStatus::unsupported, (Device * device, GraphicsAllocation &gfxAllocation));
    ADDMETHOD_NOBASE(isResident, MemoryOperationsStatus, MemoryOperationsStatus::unsupported, (Device * device, GraphicsAllocation &gfxAllocation));
    ADDMETHOD_NOBASE(makeResidentWithinOsContext, MemoryOperationsStatus, MemoryOperationsStatus::unsupported, (OsContext * osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable, const bool forcePagingFence, const bool acquireLock));
    ADDMETHOD_NOBASE(evictWithinOsContext, MemoryOperationsStatus, MemoryOperationsStatus::unsupported, (OsContext * osContext, GraphicsAllocation &gfxAllocation));
};

class MockMemoryOperations : public MemoryOperationsHandler {
  public:
    MockMemoryOperations() {}

    MemoryOperationsStatus makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations, bool isDummyExecNeeded, const bool forcePagingFence) override {
        makeResidentCalledCount++;
        makeResidentForcePagingFenceValue = forcePagingFence;
        if (captureGfxAllocationsForMakeResident) {
            for (auto &gfxAllocation : gfxAllocations) {
                if (!gfxAllocation->getAubInfo().writeMemoryOnly) {
                    gfxAllocationsForMakeResident.push_back(gfxAllocation);
                }
            }
        }
        return MemoryOperationsStatus::success;
    }

    MemoryOperationsStatus lock(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) override {
        lockCalledCount++;
        return MemoryOperationsStatus::success;
    }

    MemoryOperationsStatus evict(Device *device, GraphicsAllocation &gfxAllocation) override {
        evictCalledCount++;
        if (captureGfxAllocationsForMakeResident) {
            auto allocIterator = std::find(gfxAllocationsForMakeResident.begin(), gfxAllocationsForMakeResident.end(), &gfxAllocation);
            if (allocIterator != gfxAllocationsForMakeResident.end()) {
                gfxAllocationsForMakeResident.erase(allocIterator);
                return MemoryOperationsStatus::success;
            } else {
                return MemoryOperationsStatus::memoryNotFound;
            }
        }
        return MemoryOperationsStatus::success;
    }
    MemoryOperationsStatus isResident(Device *device, GraphicsAllocation &gfxAllocation) override {
        isResidentCalledCount++;
        if (captureGfxAllocationsForMakeResident) {
            if (std::find(gfxAllocationsForMakeResident.begin(), gfxAllocationsForMakeResident.end(), &gfxAllocation) != gfxAllocationsForMakeResident.end()) {
                return MemoryOperationsStatus::success;
            } else {
                return MemoryOperationsStatus::memoryNotFound;
            }
        }
        return MemoryOperationsStatus::success;
    }

    MemoryOperationsStatus makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable, const bool forcePagingFence, const bool acquireLock) override {
        makeResidentCalledCount++;
        if (osContext) {
            makeResidentContextId = osContext->getContextId();
        }
        if (captureGfxAllocationsForMakeResident) {
            for (auto &gfxAllocation : gfxAllocations) {
                gfxAllocationsForMakeResident.push_back(gfxAllocation);
            }
        }
        return MemoryOperationsStatus::success;
    }
    MemoryOperationsStatus evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) override {
        evictCalledCount++;
        return MemoryOperationsStatus::success;
    }

    MemoryOperationsStatus free(Device *device, GraphicsAllocation &gfxAllocation) override {
        freeCalledCount++;

        if (captureGfxAllocationsForMakeResident) {
            auto itor = std::find(gfxAllocationsForMakeResident.begin(), gfxAllocationsForMakeResident.end(), &gfxAllocation);
            if (itor != gfxAllocationsForMakeResident.end()) {
                gfxAllocationsForMakeResident.erase(itor, itor + 1);
            }
        }

        return MemoryOperationsStatus::success;
    }

    std::vector<GraphicsAllocation *> gfxAllocationsForMakeResident{};
    int makeResidentCalledCount = 0;
    bool makeResidentForcePagingFenceValue = false;
    int evictCalledCount = 0;
    int freeCalledCount = 0;
    uint32_t isResidentCalledCount = 0;
    uint32_t lockCalledCount = 0;
    uint32_t makeResidentContextId = std::numeric_limits<uint32_t>::max();
    bool captureGfxAllocationsForMakeResident = false;
};

} // namespace NEO
