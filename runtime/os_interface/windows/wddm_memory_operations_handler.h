/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/memory_manager/memory_operations_handler.h"

#include <memory>

namespace NEO {

class Wddm;
class WddmResidentAllocationsContainer;

class WddmMemoryOperationsHandler : public MemoryOperationsHandler {
  public:
    WddmMemoryOperationsHandler(Wddm *wddm);
    ~WddmMemoryOperationsHandler() override = default;

    MemoryOperationsStatus makeResident(ArrayRef<GraphicsAllocation *> gfxAllocations) override;
    MemoryOperationsStatus evict(GraphicsAllocation &gfxAllocation) override;
    MemoryOperationsStatus isResident(GraphicsAllocation &gfxAllocation) override;

  protected:
    Wddm *wddm;
    std::unique_ptr<WddmResidentAllocationsContainer> residentAllocations;
};
} // namespace NEO
