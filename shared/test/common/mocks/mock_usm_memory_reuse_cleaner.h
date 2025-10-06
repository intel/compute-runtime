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
    bool trimOldInCachesCalled = false;
    bool callBaseStartThread = false;
    bool callBaseTrimOldInCaches = true;
};
} // namespace NEO