/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/clos_cache.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"

#include <cerrno>
#include <cstring>

namespace NEO {

CacheRegion ClosCacheReservation::reserveCache(CacheLevel cacheLevel, uint16_t numWays) {
    auto closIndex = allocEntry();
    if (closIndex == CacheRegion::None) {
        return CacheRegion::None;
    }

    auto allocWays = allocCacheWay(closIndex, cacheLevel, numWays);
    if (allocWays != numWays) {
        freeEntry(closIndex);
        return CacheRegion::None;
    }

    return closIndex;
}

CacheRegion ClosCacheReservation::freeCache(CacheLevel cacheLevel, CacheRegion closIndex) {
    allocCacheWay(closIndex, cacheLevel, 0);

    return freeEntry(closIndex);
}

CacheRegion ClosCacheReservation::allocEntry() {
    return drm.getIoctlHelper()->closAlloc();
}

CacheRegion ClosCacheReservation::freeEntry(CacheRegion closIndex) {
    return drm.getIoctlHelper()->closFree(closIndex);
}

uint16_t ClosCacheReservation::allocCacheWay(CacheRegion closIndex, CacheLevel cacheLevel, uint16_t numWays) {
    return drm.getIoctlHelper()->closAllocWays(closIndex, static_cast<uint16_t>(cacheLevel), numWays);
}

} // namespace NEO
