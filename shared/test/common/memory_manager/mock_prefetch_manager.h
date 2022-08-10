/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/prefetch_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

using namespace NEO;

class MockPrefetchManager : public PrefetchManager {
  public:
    using PrefetchManager::allocations;

    void migrateAllocationsToGpu(SVMAllocsManager &unifiedMemoryManager, Device &device) override {
        PrefetchManager::migrateAllocationsToGpu(unifiedMemoryManager, device);
        migrateAllocationsToGpuCalled = true;
    }

    bool migrateAllocationsToGpuCalled = false;
};
