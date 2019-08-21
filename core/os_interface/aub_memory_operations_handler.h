/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/memory_manager/memory_operations_handler.h"
#include "core/utilities/spinlock.h"

#include "third_party/aub_stream/headers/aub_manager.h"

#include <mutex>
#include <vector>

namespace NEO {

class AubMemoryOperationsHandler : public MemoryOperationsHandler {
  public:
    AubMemoryOperationsHandler(aub_stream::AubManager *aubManager);
    ~AubMemoryOperationsHandler() override = default;

    bool makeResident(GraphicsAllocation &gfxAllocation) override;
    bool evict(GraphicsAllocation &gfxAllocation) override;
    bool isResident(GraphicsAllocation &gfxAllocation) override;
    void setAubManager(aub_stream::AubManager *aubManager);

  protected:
    MOCKABLE_VIRTUAL std::unique_lock<SpinLock> acquireLock(SpinLock &lock) {
        return std::unique_lock<SpinLock>{lock};
    }
    aub_stream::AubManager *aubManager = nullptr;
    std::vector<GraphicsAllocation *> residentAllocations;
    SpinLock resourcesLock;
};
} // namespace NEO
