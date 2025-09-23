/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/unified_memory_reuse_cleaner.h"
namespace NEO {
struct MockUnifiedMemoryReuseCleaner : public UnifiedMemoryReuseCleaner {
  public:
    using UnifiedMemoryReuseCleaner::keepCleaning;
    using UnifiedMemoryReuseCleaner::runCleaning;
    using UnifiedMemoryReuseCleaner::svmAllocationCaches;
    using UnifiedMemoryReuseCleaner::UnifiedMemoryReuseCleaner;
    using UnifiedMemoryReuseCleaner::unifiedMemoryReuseCleanerThread;

    void trimOldInCaches() override {
        trimOldInCachesCalled = true;
        if (callBaseTrimOldInCaches) {
            UnifiedMemoryReuseCleaner::trimOldInCaches();
        }
    }
    void startThread() override {
        if (callBaseStartThread) {
            UnifiedMemoryReuseCleaner::startThread();
        }
    };
    bool isEmpty() override {
        sleepOnEmptyCachesCondVar.store(UnifiedMemoryReuseCleaner::isEmpty());
        return sleepOnEmptyCachesCondVar.load();
    };
    void clearCaches() {
        std::lock_guard<std::mutex> lock(svmAllocationCachesMutex);
        svmAllocationCaches.clear();
    }
    std::atomic_bool trimOldInCachesCalled = false;
    std::atomic_bool sleepOnEmptyCachesCondVar = false;
    bool callBaseStartThread = false;
    bool callBaseTrimOldInCaches = true;
};
} // namespace NEO
