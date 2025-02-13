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
    using CacheInfo::getRegion;
    using CacheInfo::isRegionReserved;
    using CacheInfo::reservedCacheRegionsSize;
    using CacheInfo::reserveRegion;

    MockCacheInfo(IoctlHelper &ioctlHelper, CacheReservationParameters l3CacheReservationLimits)
        : CacheInfo(ioctlHelper, l3CacheReservationLimits) {}

    ~MockCacheInfo() override = default;

    bool getCacheRegion(size_t regionSize, CacheRegion regionIndex) override {
        if (regionIndex >= CacheRegion::count) {
            return false;
        }
        if (regionSize > (l3ReservationLimits.maxSize / l3ReservationLimits.maxNumRegions)) {
            return false;
        }
        return true;
    }
};

} // namespace NEO
