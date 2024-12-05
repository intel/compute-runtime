/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/cache_info.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/debug_helpers.h"

namespace NEO {

CacheInfo::~CacheInfo() {
    // skip the defaultRegion
    constexpr auto regionStart{toUnderlying(CacheRegion::region1)};
    constexpr auto regionEnd{toUnderlying(CacheRegion::count)};

    for (auto regionIndex{regionStart}; regionIndex < regionEnd; ++regionIndex) {
        if (reservedCacheRegionsSize[regionIndex]) {
            cacheReserve.freeCache(CacheLevel::level3, toCacheRegion(regionIndex));
            reservedCacheRegionsSize[regionIndex] = 0u;
        }
    }
}

CacheRegion CacheInfo::reserveRegion(size_t cacheReservationSize) {
    uint16_t numWays = (maxReservationNumWays * cacheReservationSize) / maxReservationCacheSize;
    if (debugManager.flags.ClosNumCacheWays.get() != -1) {
        numWays = debugManager.flags.ClosNumCacheWays.get();
        cacheReservationSize = (numWays * maxReservationCacheSize) / maxReservationNumWays;
    }
    auto regionIndex = cacheReserve.reserveCache(CacheLevel::level3, numWays);
    if (regionIndex == CacheRegion::none) {
        return CacheRegion::none;
    }
    DEBUG_BREAK_IF(regionIndex == CacheRegion::defaultRegion);
    DEBUG_BREAK_IF(regionIndex >= CacheRegion::count);
    reservedCacheRegionsSize[toUnderlying(regionIndex)] = cacheReservationSize;

    return regionIndex;
}

CacheRegion CacheInfo::freeRegion(CacheRegion regionIndex) {
    if (regionIndex < CacheRegion::count && reservedCacheRegionsSize[toUnderlying(regionIndex)] > 0u) {
        reservedCacheRegionsSize[toUnderlying(regionIndex)] = 0u;
        return cacheReserve.freeCache(CacheLevel::level3, regionIndex);
    }
    return CacheRegion::none;
}

bool CacheInfo::isRegionReserved(CacheRegion regionIndex, [[maybe_unused]] size_t expectedRegionSize) const {
    if (regionIndex < CacheRegion::count && reservedCacheRegionsSize[toUnderlying(regionIndex)]) {
        if (debugManager.flags.ClosNumCacheWays.get() != -1) {
            auto numWays = debugManager.flags.ClosNumCacheWays.get();
            expectedRegionSize = (numWays * maxReservationCacheSize) / maxReservationNumWays;
        }
        DEBUG_BREAK_IF(expectedRegionSize != reservedCacheRegionsSize[toUnderlying(regionIndex)]);
        return true;
    }
    return false;
}

bool CacheInfo::getRegion(size_t regionSize, CacheRegion regionIndex) {
    if (regionIndex == CacheRegion::defaultRegion) {
        return true;
    }
    if (!isRegionReserved(regionIndex, regionSize)) {
        auto regionIdx = reserveRegion(regionSize);
        if (regionIdx == CacheRegion::none) {
            return false;
        }
        DEBUG_BREAK_IF(regionIdx != regionIndex);
    }
    return true;
}

} // namespace NEO
