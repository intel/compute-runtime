/*
 * Copyright (C) 2022-2025 Intel Corporation
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
    void migrateAllocationsToGpu(PrefetchContext &prefetchContext, SVMAllocsManager &unifiedMemoryManager, Device &device, CommandStreamReceiver &csr) override {
        PrefetchManager::migrateAllocationsToGpu(prefetchContext, unifiedMemoryManager, device, csr);
        migrateAllocationsToGpuCalled = true;
        migrateAllocationsToGpuCalledCount++;
    }

    void removeAllocations(PrefetchContext &prefetchContext) override {
        PrefetchManager::removeAllocations(prefetchContext);
        removeAllocationsCalled = true;
    }

    uint32_t migrateAllocationsToGpuCalledCount = 0;
    bool migrateAllocationsToGpuCalled = false;
    bool removeAllocationsCalled = false;
};
