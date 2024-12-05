/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/common_types.h"
#include "shared/source/os_interface/linux/clos_cache.h"
#include "shared/source/utilities/spinlock.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace NEO {

class IoctlHelper;

struct CacheInfo {
    CacheInfo(IoctlHelper &ioctlHelper, size_t maxReservationCacheSize, uint32_t maxReservationNumCacheRegions, uint16_t maxReservationNumWays)
        : maxReservationCacheSize(maxReservationCacheSize),
          maxReservationNumCacheRegions(maxReservationNumCacheRegions),
          maxReservationNumWays(maxReservationNumWays),
          cacheReserve{ioctlHelper} {

        reservedCacheRegionsSize.fill(0UL);
    }

    MOCKABLE_VIRTUAL ~CacheInfo();

    CacheInfo(const CacheInfo &) = delete;
    CacheInfo &operator=(const CacheInfo &) = delete;

    size_t getMaxReservationCacheSize() const {
        return maxReservationCacheSize;
    }

    size_t getMaxReservationNumCacheRegions() const {
        return maxReservationNumCacheRegions;
    }

    size_t getMaxReservationNumWays() const {
        return maxReservationNumWays;
    }

    CacheRegion reserveCacheRegion(size_t cacheReservationSize) {
        std::unique_lock<SpinLock> lock{mtx};
        return reserveRegion(cacheReservationSize);
    }

    CacheRegion freeCacheRegion(CacheRegion regionIndex) {
        std::unique_lock<SpinLock> lock{mtx};
        return freeRegion(regionIndex);
    }

    MOCKABLE_VIRTUAL bool getCacheRegion(size_t regionSize, CacheRegion regionIndex) {
        std::unique_lock<SpinLock> lock{mtx};
        return getRegion(regionSize, regionIndex);
    }

  protected:
    CacheRegion reserveRegion(size_t cacheReservationSize);
    CacheRegion freeRegion(CacheRegion regionIndex);
    bool isRegionReserved(CacheRegion regionIndex, [[maybe_unused]] size_t regionSize) const;
    bool getRegion(size_t regionSize, CacheRegion regionIndex);

  protected:
    size_t maxReservationCacheSize;
    uint32_t maxReservationNumCacheRegions;
    uint16_t maxReservationNumWays;
    ClosCacheReservation cacheReserve;
    std::array<size_t, toUnderlying(CacheRegion::count)> reservedCacheRegionsSize;
    SpinLock mtx;
};

} // namespace NEO
