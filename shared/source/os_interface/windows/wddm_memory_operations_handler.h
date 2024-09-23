/*
 * Copyright (C) 2019-2024 Intel Corporation
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

    MemoryOperationsStatus makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations, bool isDummyExecNeeded) override;
    MemoryOperationsStatus evict(Device *device, GraphicsAllocation &gfxAllocation) override;
    MemoryOperationsStatus isResident(Device *device, GraphicsAllocation &gfxAllocation) override;

    MemoryOperationsStatus lock(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) override {
        return MemoryOperationsStatus::unsupported;
    }
    MemoryOperationsStatus makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable) override {
        return makeResident(nullptr, gfxAllocations, false);
    }
    MemoryOperationsStatus evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) override {
        return evict(nullptr, gfxAllocation);
    }

  protected:
    Wddm *wddm;
    std::unique_ptr<WddmResidentAllocationsContainer> residentAllocations;
};
} // namespace NEO
