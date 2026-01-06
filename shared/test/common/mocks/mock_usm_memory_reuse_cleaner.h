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
    bool trimOldInCachesCalled = false;
    bool callBaseTrimOldInCaches = true;

    void startThread() override {
        startThreadCalled = true;
        if (callBaseStartThread) {
            UnifiedMemoryReuseCleaner::startThread();
        }
    };
    void wait(std::unique_lock<std::mutex> &lock) override {
        waitOnConditionVar.store(!waitPredicate());
        UnifiedMemoryReuseCleaner::wait(lock);
    };
    void waitTillSleep() {
        do {
            std::this_thread::yield();
            std::lock_guard<std::mutex> lock(condVarMutex);
        } while (!waitOnConditionVar.load());
    }
    void clearCaches() {
        std::lock_guard<std::mutex> lock(svmAllocationCachesMutex);
        svmAllocationCaches.clear();
    }
    std::atomic_bool waitOnConditionVar = false;
    bool startThreadCalled = false;
    bool callBaseStartThread = false;

    void stopThread() override {
        stopThreadCalled = true;
        UnifiedMemoryReuseCleaner::stopThread();
    };
    bool stopThreadCalled = false;
};
} // namespace NEO
