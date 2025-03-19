/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/memory_operations_handler.h"

#include <memory>

namespace NEO {

struct RootDeviceEnvironment;
class Wddm;
class WddmResidentAllocationsContainer;

class WddmMemoryOperationsHandler : public MemoryOperationsHandler {
  public:
    WddmMemoryOperationsHandler(Wddm *wddm);
    ~WddmMemoryOperationsHandler() override;

    static std::unique_ptr<WddmMemoryOperationsHandler> create(Wddm *wddm, RootDeviceEnvironment *rootDeviceEnvironment, bool withAubDump);

    MemoryOperationsStatus makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations, bool isDummyExecNeeded, const bool forcePagingFence) override;
    MemoryOperationsStatus evict(Device *device, GraphicsAllocation &gfxAllocation) override;
    MemoryOperationsStatus isResident(Device *device, GraphicsAllocation &gfxAllocation) override;

    MemoryOperationsStatus lock(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) override {
        return MemoryOperationsStatus::unsupported;
    }
    MemoryOperationsStatus makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable, const bool forcePagingFence, const bool acquireLock) override {
        return makeResident(nullptr, gfxAllocations, false, forcePagingFence);
    }
    MemoryOperationsStatus evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) override {
        return evict(nullptr, gfxAllocation);
    }
    MemoryOperationsStatus free(Device *device, GraphicsAllocation &gfxAllocation) override;

  protected:
    Wddm *wddm;
    std::unique_ptr<WddmResidentAllocationsContainer> residentAllocations;
};
} // namespace NEO
