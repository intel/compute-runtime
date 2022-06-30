/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/cache_info.h"

namespace NEO {

struct MockCacheInfo : public CacheInfo {
    using CacheInfo::cacheRegionsReserved;
    using CacheInfo::isRegionReserved;

    MockCacheInfo(Drm &drm, size_t maxReservationCacheSize, uint32_t maxReservationNumCacheRegions, uint16_t maxReservationNumWays)
        : CacheInfo(drm, maxReservationCacheSize, maxReservationNumCacheRegions, maxReservationNumWays) {}

    ~MockCacheInfo() override = default;

    bool getCacheRegion(size_t regionSize, CacheRegion regionIndex) override {
        if (regionIndex >= CacheRegion::Count) {
            return false;
        }
        if (regionSize > (maxReservationCacheSize / maxReservationNumCacheRegions)) {
            return false;
        }
        return true;
    }
};

} // namespace NEO
