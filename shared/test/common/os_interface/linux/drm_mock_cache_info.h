/*
 * Copyright (C) 2021-2025 Intel Corporation
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
    using CacheInfo::getMaxReservationCacheSize;
    using CacheInfo::getRegion;
    using CacheInfo::isRegionReserved;
    using CacheInfo::l2ReservationLimits;
    using CacheInfo::l3ReservationLimits;
    using CacheInfo::reservedCacheRegionsSize;
    using CacheInfo::reserveRegion;

    MockCacheInfo(IoctlHelper &ioctlHelper, CacheReservationParameters l2CacheReservationLimits, CacheReservationParameters l3CacheReservationLimits)
        : CacheInfo(ioctlHelper, l2CacheReservationLimits, l3CacheReservationLimits) {}

    ~MockCacheInfo() override = default;

    bool getCacheRegion(size_t regionSize, CacheRegion regionIndex) override {
        switch (regionIndex) {
        case CacheRegion::defaultRegion:
        case CacheRegion::region1:
        case CacheRegion::region2:
            UNRECOVERABLE_IF(l3ReservationLimits.maxNumRegions == 0U);
            return (regionSize <= (l3ReservationLimits.maxSize / l3ReservationLimits.maxNumRegions));
        case CacheRegion::region3:
            return CacheInfo::getCacheRegion(regionSize, regionIndex);
        default:
            return false;
        }
    }
};

} // namespace NEO
