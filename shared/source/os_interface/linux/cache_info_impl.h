/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/linux/cache_info.h"
#include "shared/source/os_interface/linux/clos_cache.h"
#include "shared/source/utilities/spinlock.h"

#include <unordered_map>

namespace NEO {

class Drm;

enum class CacheLevel : uint16_t {
    Default = 0,
    Level3 = 3
};

class CacheInfoImpl : public CacheInfo {
  public:
    CacheInfoImpl(Drm &drm, size_t maxReservationCacheSize, uint32_t maxReservationNumCacheRegions, uint16_t maxReservationNumWays)
        : maxReservationCacheSize(maxReservationCacheSize), maxReservationNumCacheRegions(maxReservationNumCacheRegions), maxReservationNumWays(maxReservationNumWays), cacheReserve(drm) {
    }

    ~CacheInfoImpl() override {
        for (auto const &cacheRegion : cacheRegionsReserved) {
            cacheReserve.freeCache(CacheLevel::Level3, cacheRegion.first);
        }
        cacheRegionsReserved.clear();
    }

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

    bool getCacheRegion(size_t regionSize, CacheRegion regionIndex) override {
        std::unique_lock<SpinLock> lock{mtx};
        return getRegion(regionSize, regionIndex);
    }

  protected:
    CacheRegion reserveRegion(size_t cacheReservationSize) {
        uint16_t numWays = (maxReservationNumWays * cacheReservationSize) / maxReservationCacheSize;
        if (DebugManager.flags.ClosNumCacheWays.get() != -1) {
            numWays = DebugManager.flags.ClosNumCacheWays.get();
            cacheReservationSize = (numWays * maxReservationCacheSize) / maxReservationNumWays;
        }
        auto regionIndex = cacheReserve.reserveCache(CacheLevel::Level3, numWays);
        if (regionIndex == CacheRegion::None) {
            return CacheRegion::None;
        }
        cacheRegionsReserved.insert({regionIndex, cacheReservationSize});

        return regionIndex;
    }

    CacheRegion freeRegion(CacheRegion regionIndex) {
        auto search = cacheRegionsReserved.find(regionIndex);
        if (search != cacheRegionsReserved.end()) {
            cacheRegionsReserved.erase(search);
            return cacheReserve.freeCache(CacheLevel::Level3, regionIndex);
        }
        return CacheRegion::None;
    }

    bool isRegionReserved(CacheRegion regionIndex, [[maybe_unused]] size_t regionSize) const {
        auto search = cacheRegionsReserved.find(regionIndex);
        if (search != cacheRegionsReserved.end()) {
            if (DebugManager.flags.ClosNumCacheWays.get() != -1) {
                auto numWays = DebugManager.flags.ClosNumCacheWays.get();
                regionSize = (numWays * maxReservationCacheSize) / maxReservationNumWays;
            }
            DEBUG_BREAK_IF(search->second != regionSize);
            return true;
        }
        return false;
    }

    bool getRegion(size_t regionSize, CacheRegion regionIndex) {
        if (regionIndex == CacheRegion::Default) {
            return true;
        }
        if (!isRegionReserved(regionIndex, regionSize)) {
            auto regionIdx = reserveRegion(regionSize);
            if (regionIdx == CacheRegion::None) {
                return false;
            }
            DEBUG_BREAK_IF(regionIdx != regionIndex);
        }
        return true;
    }

    size_t maxReservationCacheSize;
    uint32_t maxReservationNumCacheRegions;
    uint16_t maxReservationNumWays;
    ClosCacheReservation cacheReserve;
    std::unordered_map<CacheRegion, size_t> cacheRegionsReserved;
    SpinLock mtx;
};

} // namespace NEO
