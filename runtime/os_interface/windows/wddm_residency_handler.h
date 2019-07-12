/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/memory_manager/residency_handler.h"

#include <memory>

namespace NEO {

class Wddm;
class WddmResidentAllocationsContainer;

class WddmResidencyHandler : public ResidencyHandler {
  public:
    WddmResidencyHandler(Wddm *wddm);
    ~WddmResidencyHandler() override = default;

    bool makeResident(GraphicsAllocation &gfxAllocation) override;
    bool evict(GraphicsAllocation &gfxAllocation) override;
    bool isResident(GraphicsAllocation &gfxAllocation) override;

  protected:
    Wddm *wddm;
    std::unique_ptr<WddmResidentAllocationsContainer> residentAllocations;
};
} // namespace NEO
