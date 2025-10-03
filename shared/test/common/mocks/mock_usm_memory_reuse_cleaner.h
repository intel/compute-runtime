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
        } else {
            clearCaches();
        }
    }
    void startThread() override {
        if (callBaseStartThread) {
            UnifiedMemoryReuseCleaner::startThread();
        }
    };
    void wait(std::unique_lock<std::mutex> &lock) override {
        waitOnConditionVar.store(true);
        UnifiedMemoryReuseCleaner::wait(lock);
    };
    void clearCaches() {
        std::lock_guard<std::mutex> lock(svmAllocationCachesMutex);
        svmAllocationCaches.clear();
    }
    std::atomic_bool trimOldInCachesCalled = false;
    std::atomic_bool waitOnConditionVar = false;
    bool callBaseStartThread = false;
    bool callBaseTrimOldInCaches = true;
};
} // namespace NEO
