/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/linux/clos_cache.h"
#include "shared/source/utilities/spinlock.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace NEO {

class IoctlHelper;

struct CacheReservationParameters {
    size_t maxSize{0U};
    uint32_t maxNumRegions{0U};
    uint16_t maxNumWays{0U};
};

struct CacheInfo : NEO::NonCopyableAndNonMovableClass {
    CacheInfo(IoctlHelper &ioctlHelper, const CacheReservationParameters l2Limits, const CacheReservationParameters l3Limits)
        : l2ReservationLimits{l2Limits},
          l3ReservationLimits{l3Limits},
          cacheReserve{ioctlHelper} {

        reservedCacheRegionsSize.fill(0UL);
    }

    MOCKABLE_VIRTUAL ~CacheInfo();

    size_t getMaxReservationCacheSize(CacheLevel cacheLevel) const {
        return getLimitsForCacheLevel(cacheLevel).maxSize;
    }

    size_t getMaxReservationNumCacheRegions(CacheLevel cacheLevel) const {
        return getLimitsForCacheLevel(cacheLevel).maxNumRegions;
    }

    size_t getMaxReservationNumWays(CacheLevel cacheLevel) const {
        return getLimitsForCacheLevel(cacheLevel).maxNumWays;
    }

    CacheRegion reserveCacheRegion(CacheLevel cacheLevel, size_t cacheReservationSize) {
        std::unique_lock<SpinLock> lock{mtx};
        return reserveRegion(cacheLevel, cacheReservationSize);
    }

    CacheRegion freeCacheRegion(CacheLevel cacheLevel, CacheRegion regionIndex) {
        std::unique_lock<SpinLock> lock{mtx};
        return freeRegion(cacheLevel, regionIndex);
    }

    MOCKABLE_VIRTUAL bool getCacheRegion(size_t regionSize, CacheRegion regionIndex) {
        std::unique_lock<SpinLock> lock{mtx};
        return getRegion(regionSize, regionIndex);
    }

    CacheLevel getLevelForRegion(CacheRegion regionIndex) const {
        DEBUG_BREAK_IF(regionIndex == CacheRegion::defaultRegion);
        DEBUG_BREAK_IF(regionIndex >= CacheRegion::count);
        return (regionIndex == CacheRegion::region3 ? CacheLevel::level2 : CacheLevel::level3);
    }

    CacheLevel getLevelForRegion(uint16_t regionIndex) const {
        return getLevelForRegion(toCacheRegion(regionIndex));
    }

  protected:
    CacheRegion reserveRegion(CacheLevel cacheLevel, size_t cacheReservationSize);
    CacheRegion freeRegion(CacheLevel cacheLevel, CacheRegion regionIndex);
    bool isRegionReserved(CacheRegion regionIndex, [[maybe_unused]] size_t regionSize) const;
    bool getRegion(size_t regionSize, CacheRegion regionIndex);
    const CacheReservationParameters &getLimitsForCacheLevel(CacheLevel cacheLevel) const;

  protected:
    CacheReservationParameters l2ReservationLimits{};
    CacheReservationParameters l3ReservationLimits{};
    ClosCacheReservation cacheReserve;
    std::array<size_t, toUnderlying(CacheRegion::count)> reservedCacheRegionsSize;
    SpinLock mtx;
};

static_assert(NEO::NonCopyableAndNonMovable<CacheInfo>);

} // namespace NEO
