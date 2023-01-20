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
    void migrateAllocationsToGpu(PrefetchContext &prefetchContext, SVMAllocsManager &unifiedMemoryManager, Device &device, CommandStreamReceiver &csr) override {
        PrefetchManager::migrateAllocationsToGpu(prefetchContext, unifiedMemoryManager, device, csr);
        migrateAllocationsToGpuCalled = true;
    }

    void removeAllocations(PrefetchContext &prefetchContext) override {
        PrefetchManager::removeAllocations(prefetchContext);
        removeAllocationsCalled = true;
    }

    bool migrateAllocationsToGpuCalled = false;
    bool removeAllocationsCalled = false;
};
