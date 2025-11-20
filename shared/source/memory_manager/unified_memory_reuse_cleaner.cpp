/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_reuse_cleaner.h"

#include "shared/source/helpers/sleep.h"
#include "shared/source/memory_manager/deferred_deleter.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_thread.h"

#include <thread>
namespace NEO {

UnifiedMemoryReuseCleaner::UnifiedMemoryReuseCleaner(bool trimAllAllocations) : trimAllAllocations(trimAllAllocations) {
}

UnifiedMemoryReuseCleaner::~UnifiedMemoryReuseCleaner() {
    UNRECOVERABLE_IF(this->unifiedMemoryReuseCleanerThread);
}

void UnifiedMemoryReuseCleaner::stopThread() {
    {
        std::lock_guard<std::mutex> lock(condVarMutex);
        keepCleaning.store(false);
        condVar.notify_one();
    }
    if (unifiedMemoryReuseCleanerThread) {
        unifiedMemoryReuseCleanerThread->join();
        unifiedMemoryReuseCleanerThread.reset();
    }
};

void *UnifiedMemoryReuseCleaner::cleanUnifiedMemoryReuse(void *self) {
    auto cleaner = reinterpret_cast<UnifiedMemoryReuseCleaner *>(self);
    while (cleaner->keepCleaning.load()) {
        std::unique_lock lock(cleaner->condVarMutex);
        cleaner->wait(lock);
        lock.unlock();
        cleaner->trimOldInCaches();
        NEO::sleep(sleepTime);
    }
    return nullptr;
}

void UnifiedMemoryReuseCleaner::notifySvmAllocationsCacheUpdate() {
    std::lock_guard<std::mutex> lock(condVarMutex);
    condVar.notify_one();
}

void UnifiedMemoryReuseCleaner::registerSvmAllocationCache(SvmAllocationCache *cache) {
    std::lock_guard<std::mutex> lockSvmAllocationCaches(this->svmAllocationCachesMutex);
    this->svmAllocationCaches.push_back(cache);
}

void UnifiedMemoryReuseCleaner::unregisterSvmAllocationCache(SvmAllocationCache *cache) {
    std::lock_guard<std::mutex> lockSvmAllocationCaches(this->svmAllocationCachesMutex);
    this->svmAllocationCaches.erase(std::find(this->svmAllocationCaches.begin(), this->svmAllocationCaches.end(), cache));
}

void UnifiedMemoryReuseCleaner::trimOldInCaches() {
    bool shouldLimitReuse = false;
    auto trimTimePoint = std::chrono::high_resolution_clock::now() - maxHoldTime;
    std::lock_guard<std::mutex> lockSvmAllocationCaches(this->svmAllocationCachesMutex);
    for (auto svmAllocCache : this->svmAllocationCaches) {
        shouldLimitReuse |= svmAllocCache->memoryManager->shouldLimitAllocationsReuse();
        if (shouldLimitReuse) {
            trimTimePoint = std::chrono::high_resolution_clock::now() - limitedHoldTime;
        } else {
            if (auto deferredDeleter = svmAllocCache->memoryManager->getDeferredDeleter()) {
                if (false == deferredDeleter->areElementsReleased(false)) {
                    continue;
                }
            }
        }
        svmAllocCache->trimOldAllocs(trimTimePoint, shouldLimitReuse || this->trimAllAllocations);
    }
}

void UnifiedMemoryReuseCleaner::startThread() {
    std::call_once(startThreadOnce, [this]() {
        this->unifiedMemoryReuseCleanerThread = Thread::createFunc(cleanUnifiedMemoryReuse, reinterpret_cast<void *>(this));
    });
}

} // namespace NEO
