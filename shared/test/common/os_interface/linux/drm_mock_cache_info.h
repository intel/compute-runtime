/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/cache_info_impl.h"

namespace NEO {

struct MockCacheInfoImpl : public CacheInfoImpl {
    using CacheInfoImpl::cacheRegionsReserved;
    using CacheInfoImpl::isRegionReserved;

    MockCacheInfoImpl(Drm &drm, size_t maxReservationCacheSize, uint32_t maxReservationNumCacheRegions, uint16_t maxReservationNumWays)
        : CacheInfoImpl(drm, maxReservationCacheSize, maxReservationNumCacheRegions, maxReservationNumWays) {}

    ~MockCacheInfoImpl() override = default;

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
