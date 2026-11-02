/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/allocation_reuse_cache.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/unified_memory/unified_memory.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

namespace NEO {

struct SVMAllocsManager::SvmCacheAllocationInfo {
    size_t allocationSize;
    void *allocation;
    SvmAllocationData *svmData;
    std::chrono::high_resolution_clock::time_point saveTime;
    bool isInUseCheckRequired;
    SvmCacheAllocationInfo(size_t allocationSize, void *allocation, SvmAllocationData *svmData, bool isInUseCheckRequired) : allocationSize(allocationSize), allocation(allocation), svmData(svmData), isInUseCheckRequired(isInUseCheckRequired) {
        saveTime = std::chrono::high_resolution_clock::now();
    }
    bool operator<(SvmCacheAllocationInfo const &other) const {
        return allocationSize < other.allocationSize;
    }
    bool operator<(size_t const &size) const {
        return allocationSize < size;
    }
    void markForDelete() {
        allocationSize = 0u;
    }
    static bool isMarkedForDelete(SvmCacheAllocationInfo const &info) {
        return 0 == info.allocationSize;
    }
};

struct SVMAllocsManager::SvmAllocationCache : AllocationReuseCache {
    enum class CacheOperationType {
        insert,
        get,
        trim,
        trimOld
    };

    struct SvmAllocationCachePerfInfo {
        uint64_t allocationSize;
        std::chrono::high_resolution_clock::time_point timePoint;
        InternalMemoryType allocationType;
        CacheOperationType operationType;
        bool isSuccess;
    };

    static constexpr size_t maxServicedSize = 256 * MemoryConstants::megaByte;
    static constexpr size_t minimalSizeToCheckUtilization = 4 * MemoryConstants::pageSize64k;
    static constexpr double minimalAllocUtilization = 0.5;

    SvmAllocationCache();

    static bool sizeAllowed(size_t size) { return size <= SvmAllocationCache::maxServicedSize; }
    UsmReuseInfo &getUsmReuseInfo(SvmAllocationData &svmData);
    bool insert(size_t size, void *ptr, SvmAllocationData *svmData, FreePolicyType freePolicy);
    static bool allocUtilizationAllows(size_t requestedSize, size_t reuseCandidateSize);
    static bool alignmentAllows(void *ptr, size_t alignment);
    bool isInUse(SvmCacheAllocationInfo &cacheAllocInfo);
    bool isEmpty() override { return empty; };
    void *get(size_t size, const UnifiedMemoryProperties &unifiedMemoryProperties);
    void *get(size_t size, const UnifiedMemoryProperties &unifiedMemoryProperties, std::optional<uint32_t> storageOwnerRootDeviceIndex);
    void trim();
    void trimOldAllocs(std::chrono::high_resolution_clock::time_point trimTimePoint, bool trimAll) override;
    MemoryManager *getMemoryManager() const override { return memoryManager; }
    void cleanup();
    void logCacheOperation(const SvmAllocationCachePerfInfo &cachePerfEvent) const;

    std::vector<SvmCacheAllocationInfo> allocations;

    std::mutex mtx;
    std::mutex trimMtx;
    SVMAllocsManager *svmAllocsManager = nullptr;
    MemoryManager *memoryManager = nullptr;
    bool enablePerformanceLogging = false;
    bool requireUpdatingAllocsForIndirectAccess = false;
    bool usesSharedUsmReuseInfo = false;
    size_t allocationSizeAlignment = 1u;
    std::atomic_bool empty = true;
};

} // namespace NEO
