/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <vector>
namespace NEO {
class Thread;
class UnifiedMemoryReuseCleaner : NEO::NonCopyableAndNonMovableClass {
    using SvmAllocationCache = SVMAllocsManager::SvmAllocationCache;

  public:
    static constexpr auto sleepTime = std::chrono::milliseconds(15u);
    static constexpr auto maxHoldTime = std::chrono::seconds(10u);
    static constexpr auto limitedHoldTime = std::chrono::seconds(2u);
    UnifiedMemoryReuseCleaner(bool trimAllAllocations);
    virtual ~UnifiedMemoryReuseCleaner();

    MOCKABLE_VIRTUAL void startThread();
    void stopThread();

    static bool isSupported();

    void registerSvmAllocationCache(SvmAllocationCache *cache);
    void unregisterSvmAllocationCache(SvmAllocationCache *cache);

  protected:
    void startCleaning() { runCleaning.store(true); };
    static void *cleanUnifiedMemoryReuse(void *self);
    MOCKABLE_VIRTUAL void trimOldInCaches();
    std::unique_ptr<Thread> unifiedMemoryReuseCleanerThread;

    std::vector<SvmAllocationCache *> svmAllocationCaches;
    std::mutex svmAllocationCachesMutex;

    std::atomic_bool runCleaning = false;
    std::atomic_bool keepCleaning = true;

    bool trimAllAllocations = false;
};

static_assert(NEO::NonCopyableAndNonMovable<UnifiedMemoryReuseCleaner>);

} // namespace NEO
