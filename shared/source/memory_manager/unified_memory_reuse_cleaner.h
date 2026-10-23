/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/memory_manager/allocation_reuse_cache.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>
namespace NEO {
class Thread;
class UnifiedMemoryReuseCleaner : NEO::NonCopyableAndNonMovableClass {
  public:
    static constexpr auto sleepTime = std::chrono::milliseconds(15u);
    static constexpr auto maxHoldTime = std::chrono::seconds(10u);
    static constexpr auto limitedHoldTime = std::chrono::seconds(2u);
    UnifiedMemoryReuseCleaner(bool trimAllAllocations);
    virtual ~UnifiedMemoryReuseCleaner();

    MOCKABLE_VIRTUAL void startThread();
    MOCKABLE_VIRTUAL void stopThread();

    static bool isSupported();

    void registerAllocationCache(AllocationReuseCache *cache);
    void unregisterAllocationCache(AllocationReuseCache *cache);
    bool waitPredicate() { return !keepCleaning || !isEmpty(); };
    MOCKABLE_VIRTUAL void wait(std::unique_lock<std::mutex> &lock) {
        condVar.wait(lock, [&]() { return waitPredicate(); });
    }
    MOCKABLE_VIRTUAL bool isEmpty() {
        std::unique_lock<std::mutex> lock(allocationCachesMutex);
        return std::all_of(allocationCaches.begin(), allocationCaches.end(), [](const auto &it) { return it->isEmpty(); });
    }
    void notifyAllocationCacheUpdate();

  protected:
    static void *cleanUnifiedMemoryReuse(void *self);
    MOCKABLE_VIRTUAL void trimOldInCaches();
    std::unique_ptr<Thread> unifiedMemoryReuseCleanerThread;

    std::vector<AllocationReuseCache *> allocationCaches;
    std::mutex allocationCachesMutex;
    std::once_flag startThreadOnce;

    std::mutex condVarMutex;
    std::condition_variable condVar;
    std::atomic_bool keepCleaning = true;

    bool trimAllAllocations = false;
};

static_assert(NEO::NonCopyableAndNonMovable<UnifiedMemoryReuseCleaner>);

} // namespace NEO
