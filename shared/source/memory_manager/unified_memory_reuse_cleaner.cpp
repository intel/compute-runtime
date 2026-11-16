/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_reuse_cleaner.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/sleep.h"
#include "shared/source/memory_manager/deferred_deleter.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_thread.h"

#include <algorithm>
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

void UnifiedMemoryReuseCleaner::notifyAllocationCacheUpdate() {
    std::lock_guard<std::mutex> lock(condVarMutex);
    condVar.notify_one();
}

void UnifiedMemoryReuseCleaner::registerAllocationCache(AllocationReuseCache *cache) {
    std::lock_guard<std::mutex> lockAllocationCaches(this->allocationCachesMutex);
    this->allocationCaches.push_back(cache);
}

void UnifiedMemoryReuseCleaner::unregisterAllocationCache(AllocationReuseCache *cache) {
    std::lock_guard<std::mutex> lockAllocationCaches(this->allocationCachesMutex);
    std::erase(this->allocationCaches, cache);
}

void UnifiedMemoryReuseCleaner::trimOldInCaches() {
    const auto now = std::chrono::high_resolution_clock::now();
    std::lock_guard<std::mutex> lockAllocationCaches(this->allocationCachesMutex);
    if (this->trimAllAllocations && this->isAnyDirectSubmissionLightRingRunning() && !this->isAnyCacheUnderMemoryPressure()) {
        return;
    }
    for (auto allocationCache : this->allocationCaches) {
        allocationCache->trimOldAllocsUnderMemoryPressure(now - limitedHoldTime);
        if (auto deferredDeleter = allocationCache->getMemoryManager()->getDeferredDeleter()) {
            if (false == deferredDeleter->areElementsReleased(false)) {
                continue;
            }
        }
        allocationCache->trimOldAllocs(now - maxHoldTime, this->trimAllAllocations);
    }
}

bool UnifiedMemoryReuseCleaner::isAnyCacheUnderMemoryPressure() const {
    return std::any_of(this->allocationCaches.begin(), this->allocationCaches.end(), [](const auto allocationCache) {
        return allocationCache->getMemoryManager()->shouldLimitAllocationsReuse();
    });
}

bool UnifiedMemoryReuseCleaner::isAnyDirectSubmissionLightRingRunning() const {
    if (this->allocationCaches.empty()) {
        return false;
    }
    for (const auto &engines : this->allocationCaches[0]->getMemoryManager()->getRegisteredEngines()) {
        for (const auto &engine : engines) {
            if (!engine.osContext->isDirectSubmissionLightActive()) {
                continue;
            }
            auto lock = engine.commandStreamReceiver->tryObtainUniqueOwnership();
            if (!lock.owns_lock() || engine.commandStreamReceiver->isDirectSubmissionRingRunning()) {
                return true;
            }
        }
    }
    return false;
}

void UnifiedMemoryReuseCleaner::startThread() {
    std::call_once(startThreadOnce, [this]() {
        this->unifiedMemoryReuseCleanerThread = Thread::createFunc(cleanUnifiedMemoryReuse, reinterpret_cast<void *>(this));
    });
}

} // namespace NEO
