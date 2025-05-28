/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/local_ids_cache.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/local_id_gen.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/kernel/grf_config.h"

#include <cstring>

namespace NEO {

LocalIdsCache::LocalIdsCache(size_t cacheSize, std::array<uint8_t, 3> wgDimOrder, uint32_t grfCount, uint8_t simdSize, uint8_t grfSize, bool usesOnlyImages)
    : wgDimOrder(wgDimOrder), localIdsSizePerThread(getPerThreadSizeLocalIDs(static_cast<uint32_t>(simdSize), static_cast<uint32_t>(grfSize))),
      grfCount(grfCount), grfSize(grfSize), simdSize(simdSize), usesOnlyImages(usesOnlyImages) {
    UNRECOVERABLE_IF(cacheSize == 0)
    cache.resize(cacheSize);
}

LocalIdsCache::~LocalIdsCache() {
    for (auto &cacheEntry : cache) {
        alignedFree(cacheEntry.localIdsData);
    }
}

std::unique_lock<std::mutex> LocalIdsCache::lock() {
    return std::unique_lock<std::mutex>(setLocalIdsMutex);
}

size_t LocalIdsCache::getLocalIdsSizeForGroup(const Vec3<uint16_t> &group, const RootDeviceEnvironment &rootDeviceEnvironment) const {
    const auto numElementsInGroup = static_cast<uint32_t>(Math::computeTotalElementsCount({group[0], group[1], group[2]}));
    if (isSimd1(simdSize)) {
        return static_cast<size_t>(numElementsInGroup * localIdsSizePerThread);
    }
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<NEO::GfxCoreHelper>();
    const auto numberOfThreads = gfxCoreHelper.calculateNumThreadsPerThreadGroup(simdSize, numElementsInGroup, grfCount, rootDeviceEnvironment);
    return static_cast<size_t>(numberOfThreads * localIdsSizePerThread);
}

size_t LocalIdsCache::getLocalIdsSizePerThread() const {
    return localIdsSizePerThread;
}

void LocalIdsCache::setLocalIdsForEntry(LocalIdsCacheEntry &entry, void *destination) {
    entry.accessCounter++;
    std::memcpy(destination, entry.localIdsData, entry.localIdsSize);
}

void LocalIdsCache::setLocalIdsForGroup(const Vec3<uint16_t> &group, void *destination, const RootDeviceEnvironment &rootDeviceEnvironment) {
    auto setLocalIdsLock = lock();
    LocalIdsCacheEntry *leastAccessedEntry = &cache[0];
    for (auto &cacheEntry : cache) {
        if (cacheEntry.groupSize == group) {
            return setLocalIdsForEntry(cacheEntry, destination);
        }

        if (cacheEntry.accessCounter < leastAccessedEntry->accessCounter) {
            leastAccessedEntry = &cacheEntry;
        }
    }

    commitNewEntry(*leastAccessedEntry, group, rootDeviceEnvironment);
    setLocalIdsForEntry(*leastAccessedEntry, destination);
}

void LocalIdsCache::commitNewEntry(LocalIdsCacheEntry &entry, const Vec3<uint16_t> &group, const RootDeviceEnvironment &rootDeviceEnvironment) {
    entry.localIdsSize = getLocalIdsSizeForGroup(group, rootDeviceEnvironment);
    entry.groupSize = group;
    entry.accessCounter = 0U;
    if (entry.localIdsSize > entry.localIdsSizeAllocated) {
        alignedFree(entry.localIdsData);
        entry.localIdsData = static_cast<uint8_t *>(alignedMalloc(entry.localIdsSize, 32));
        entry.localIdsSizeAllocated = entry.localIdsSize;
    }
    NEO::generateLocalIDs(entry.localIdsData, static_cast<uint16_t>(simdSize),
                          {group[0], group[1], group[2]}, wgDimOrder, usesOnlyImages, grfSize, grfCount, rootDeviceEnvironment);
}

} // namespace NEO