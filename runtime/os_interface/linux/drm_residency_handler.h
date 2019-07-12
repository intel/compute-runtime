/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/memory_manager/residency_handler.h"

namespace NEO {

class DrmResidencyHandler : public ResidencyHandler {
  public:
    DrmResidencyHandler();
    ~DrmResidencyHandler() override = default;

    bool makeResident(GraphicsAllocation &gfxAllocation) override;
    bool evict(GraphicsAllocation &gfxAllocation) override;
    bool isResident(GraphicsAllocation &gfxAllocation) override;
};
} // namespace NEO
