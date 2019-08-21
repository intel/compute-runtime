/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/memory_manager/memory_operations_handler.h"

namespace NEO {

class DrmMemoryOperationsHandler : public MemoryOperationsHandler {
  public:
    DrmMemoryOperationsHandler();
    ~DrmMemoryOperationsHandler() override = default;

    bool makeResident(GraphicsAllocation &gfxAllocation) override;
    bool evict(GraphicsAllocation &gfxAllocation) override;
    bool isResident(GraphicsAllocation &gfxAllocation) override;
};
} // namespace NEO
