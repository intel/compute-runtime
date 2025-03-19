/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/aub_memory_operations_handler.h"
#include "shared/source/utilities/arrayref.h"

namespace NEO {

class Device;
class GraphicsAllocation;
class OsContext;

struct MockAubMemoryOperationsHandler : public AubMemoryOperationsHandler {
    using AubMemoryOperationsHandler::AubMemoryOperationsHandler;
    using AubMemoryOperationsHandler::getMemoryBanksBitfield;
    using AubMemoryOperationsHandler::residentAllocations;

    MemoryOperationsStatus makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations, bool isDummyExecNeeded, const bool forcePagingFence) override {
        makeResidentCalled = true;
        return AubMemoryOperationsHandler::makeResident(device, gfxAllocations, isDummyExecNeeded, forcePagingFence);
    }

    MemoryOperationsStatus evict(Device *device, GraphicsAllocation &gfxAllocation) override {
        evictCalled = true;
        return AubMemoryOperationsHandler::evict(device, gfxAllocation);
    }

    MemoryOperationsStatus isResident(Device *device, GraphicsAllocation &gfxAllocation) override {
        isResidentCalled = true;
        return AubMemoryOperationsHandler::isResident(device, gfxAllocation);
    }

    MemoryOperationsStatus makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable, const bool forcePagingFence, const bool acquireLock) override {
        makeResidentWithinOsContextCalled = true;
        return AubMemoryOperationsHandler::makeResidentWithinOsContext(osContext, gfxAllocations, evictable, forcePagingFence, acquireLock);
    }

    MemoryOperationsStatus evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) override {
        evictWithinOsContextCalled = true;
        return AubMemoryOperationsHandler::evictWithinOsContext(osContext, gfxAllocation);
    }

    MemoryOperationsStatus free(Device *device, GraphicsAllocation &gfxAllocation) override {
        freeCalled = true;
        return AubMemoryOperationsHandler::free(device, gfxAllocation);
    }

    bool makeResidentCalled = false;
    bool evictCalled = false;
    bool isResidentCalled = false;
    bool makeResidentWithinOsContextCalled = false;
    bool evictWithinOsContextCalled = false;
    bool freeCalled = false;
};

} // namespace NEO
