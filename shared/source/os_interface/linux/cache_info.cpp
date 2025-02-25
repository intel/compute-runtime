/*
 * Copyright (C) 2022-2025 Intel Corporation
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
        const auto cacheLevel{getLevelForRegion(regionIndex)};

        if (reservedCacheRegionsSize[regionIndex]) {
            cacheReserve.freeCache(cacheLevel, toCacheRegion(regionIndex));
            reservedCacheRegionsSize[regionIndex] = 0u;
        }
    }
}

CacheRegion CacheInfo::reserveRegion(CacheLevel cacheLevel, size_t cacheReservationSize) {
    auto &limits{getLimitsForCacheLevel(cacheLevel)};

    if (limits.maxSize == 0U || limits.maxNumWays == 0U || limits.maxNumRegions == 0U) {
        return CacheRegion::none;
    }

    auto numWays{static_cast<uint16_t>((limits.maxNumWays * cacheReservationSize) / limits.maxSize)};
    if (debugManager.flags.ClosNumCacheWays.get() != -1) {
        numWays = debugManager.flags.ClosNumCacheWays.get();
        cacheReservationSize = (numWays * limits.maxSize) / limits.maxNumWays;
    }

    auto regionIndex = cacheReserve.reserveCache(cacheLevel, numWays);
    if (regionIndex == CacheRegion::none) {
        return CacheRegion::none;
    }
    DEBUG_BREAK_IF(regionIndex == CacheRegion::defaultRegion);
    DEBUG_BREAK_IF(regionIndex >= CacheRegion::count);
    reservedCacheRegionsSize[toUnderlying(regionIndex)] = cacheReservationSize;

    return regionIndex;
}

CacheRegion CacheInfo::freeRegion(CacheLevel cacheLevel, CacheRegion regionIndex) {
    if (regionIndex < CacheRegion::count && reservedCacheRegionsSize[toUnderlying(regionIndex)] > 0u) {
        reservedCacheRegionsSize[toUnderlying(regionIndex)] = 0u;
        return cacheReserve.freeCache(cacheLevel, regionIndex);
    }
    return CacheRegion::none;
}

bool CacheInfo::isRegionReserved(CacheRegion regionIndex, [[maybe_unused]] size_t expectedRegionSize) const {
    DEBUG_BREAK_IF(regionIndex == CacheRegion::defaultRegion);
    DEBUG_BREAK_IF(regionIndex >= CacheRegion::count);

    const auto cacheLevel{getLevelForRegion(regionIndex)};
    auto &limits{getLimitsForCacheLevel(cacheLevel)};

    if (limits.maxSize == 0U || limits.maxNumWays == 0U || limits.maxNumRegions == 0U) {
        return false;
    }

    if (regionIndex < CacheRegion::count && reservedCacheRegionsSize[toUnderlying(regionIndex)]) {
        if (debugManager.flags.ClosNumCacheWays.get() != -1) {
            auto numWays = debugManager.flags.ClosNumCacheWays.get();
            expectedRegionSize = (numWays * limits.maxSize) / limits.maxNumWays;
        }
        DEBUG_BREAK_IF(expectedRegionSize != reservedCacheRegionsSize[toUnderlying(regionIndex)]);
        return true;
    }
    return false;
}

bool CacheInfo::getRegion(size_t regionSize, CacheRegion regionIndex) {
    DEBUG_BREAK_IF(regionIndex >= CacheRegion::count);

    if (regionIndex == CacheRegion::defaultRegion) {
        return true;
    }

    if (!isRegionReserved(regionIndex, regionSize)) {
        const auto cacheLevel{getLevelForRegion(regionIndex)};
        auto regionIdx = reserveRegion(cacheLevel, regionSize);
        if (regionIdx == CacheRegion::none) {
            return false;
        }
        DEBUG_BREAK_IF(regionIdx != regionIndex);
    }
    return true;
}

const CacheReservationParameters &CacheInfo::getLimitsForCacheLevel(CacheLevel cacheLevel) const {
    switch (cacheLevel) {
    case CacheLevel::level2:
        return l2ReservationLimits;
    case CacheLevel::level3:
        return l3ReservationLimits;
    default:
        UNRECOVERABLE_IF(true);
    }
}

} // namespace NEO
