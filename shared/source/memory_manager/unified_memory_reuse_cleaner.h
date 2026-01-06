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
#include <condition_variable>
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
    MOCKABLE_VIRTUAL void stopThread();

    static bool isSupported();

    void registerSvmAllocationCache(SvmAllocationCache *cache);
    void unregisterSvmAllocationCache(SvmAllocationCache *cache);
    bool waitPredicate() { return !keepCleaning || !isEmpty(); };
    MOCKABLE_VIRTUAL void wait(std::unique_lock<std::mutex> &lock) {
        condVar.wait(lock, [&]() { return waitPredicate(); });
    }
    MOCKABLE_VIRTUAL bool isEmpty() {
        std::unique_lock<std::mutex> lock(svmAllocationCachesMutex);
        return std::all_of(svmAllocationCaches.begin(), svmAllocationCaches.end(), [](const auto &it) { return it->isEmpty(); });
    }
    void notifySvmAllocationsCacheUpdate();

  protected:
    static void *cleanUnifiedMemoryReuse(void *self);
    MOCKABLE_VIRTUAL void trimOldInCaches();
    std::unique_ptr<Thread> unifiedMemoryReuseCleanerThread;

    std::vector<SvmAllocationCache *> svmAllocationCaches;
    std::mutex svmAllocationCachesMutex;
    std::once_flag startThreadOnce;

    std::mutex condVarMutex;
    std::condition_variable condVar;
    std::atomic_bool keepCleaning = true;

    bool trimAllAllocations = false;
};

static_assert(NEO::NonCopyableAndNonMovable<UnifiedMemoryReuseCleaner>);

} // namespace NEO
