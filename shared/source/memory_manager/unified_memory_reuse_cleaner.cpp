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
    keepCleaning.store(false);
    runCleaning.store(false);
    if (unifiedMemoryReuseCleanerThread) {
        unifiedMemoryReuseCleanerThread->join();
        unifiedMemoryReuseCleanerThread.reset();
    }
};

void *UnifiedMemoryReuseCleaner::cleanUnifiedMemoryReuse(void *self) {
    auto cleaner = reinterpret_cast<UnifiedMemoryReuseCleaner *>(self);
    while (!cleaner->runCleaning.load()) {
        if (!cleaner->keepCleaning.load()) {
            return nullptr;
        }
        NEO::sleep(sleepTime);
    }

    while (true) {
        if (!cleaner->keepCleaning.load()) {
            return nullptr;
        }
        NEO::sleep(sleepTime);
        cleaner->trimOldInCaches();
    }
}

void UnifiedMemoryReuseCleaner::registerSvmAllocationCache(SvmAllocationCache *cache) {
    std::lock_guard<std::mutex> lockSvmAllocationCaches(this->svmAllocationCachesMutex);
    this->svmAllocationCaches.push_back(cache);
    this->startCleaning();
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
    this->unifiedMemoryReuseCleanerThread = Thread::createFunc(cleanUnifiedMemoryReuse, reinterpret_cast<void *>(this));
}

} // namespace NEO