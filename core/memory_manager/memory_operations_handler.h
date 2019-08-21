/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

class GraphicsAllocation;

class MemoryOperationsHandler {
  public:
    MemoryOperationsHandler() = default;
    virtual ~MemoryOperationsHandler() = default;

    virtual bool makeResident(GraphicsAllocation &gfxAllocation) = 0;
    virtual bool evict(GraphicsAllocation &gfxAllocation) = 0;
    virtual bool isResident(GraphicsAllocation &gfxAllocation) = 0;
};
} // namespace NEO
