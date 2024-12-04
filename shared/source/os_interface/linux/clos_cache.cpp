/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/clos_cache.h"

#include "shared/source/helpers/common_types.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"

#include <cerrno>
#include <cstring>

namespace NEO {

CacheRegion ClosCacheReservation::reserveCache(CacheLevel cacheLevel, uint16_t numWays) {
    auto closIndex = allocEntry(cacheLevel);
    if (closIndex == CacheRegion::none) {
        return CacheRegion::none;
    }

    auto allocWays = allocCacheWay(closIndex, cacheLevel, numWays);
    if (allocWays != numWays) {
        freeEntry(closIndex);
        return CacheRegion::none;
    }

    return closIndex;
}

CacheRegion ClosCacheReservation::freeCache(CacheLevel cacheLevel, CacheRegion closIndex) {
    allocCacheWay(closIndex, cacheLevel, 0);

    return freeEntry(closIndex);
}

CacheRegion ClosCacheReservation::allocEntry(CacheLevel cacheLevel) {
    return ioctlHelper.closAlloc(cacheLevel);
}

CacheRegion ClosCacheReservation::freeEntry(CacheRegion closIndex) {
    return ioctlHelper.closFree(closIndex);
}

uint16_t ClosCacheReservation::allocCacheWay(CacheRegion closIndex, CacheLevel cacheLevel, uint16_t numWays) {
    return ioctlHelper.closAllocWays(closIndex, static_cast<uint16_t>(cacheLevel), numWays);
}

} // namespace NEO
