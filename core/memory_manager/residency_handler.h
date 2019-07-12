/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

class GraphicsAllocation;

class ResidencyHandler {
  public:
    ResidencyHandler() = default;
    virtual ~ResidencyHandler() = default;

    virtual bool makeResident(GraphicsAllocation &gfxAllocation) = 0;
    virtual bool evict(GraphicsAllocation &gfxAllocation) = 0;
    virtual bool isResident(GraphicsAllocation &gfxAllocation) = 0;
};
} // namespace NEO
