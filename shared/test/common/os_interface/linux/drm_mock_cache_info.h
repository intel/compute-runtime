/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/common_types.h"
#include "shared/source/os_interface/linux/cache_info.h"

namespace NEO {

struct MockCacheInfo : public CacheInfo {
    using CacheInfo::freeRegion;
    using CacheInfo::getRegion;
    using CacheInfo::isRegionReserved;
    using CacheInfo::reservedCacheRegionsSize;
    using CacheInfo::reserveRegion;

    MockCacheInfo(IoctlHelper &ioctlHelper, size_t maxReservationCacheSize, uint32_t maxReservationNumCacheRegions, uint16_t maxReservationNumWays)
        : CacheInfo(ioctlHelper, maxReservationCacheSize, maxReservationNumCacheRegions, maxReservationNumWays) {}

    ~MockCacheInfo() override = default;

    bool getCacheRegion(size_t regionSize, CacheRegion regionIndex) override {
        if (regionIndex >= CacheRegion::count) {
            return false;
        }
        if (regionSize > (maxReservationCacheSize / maxReservationNumCacheRegions)) {
            return false;
        }
        return true;
    }
};

} // namespace NEO
