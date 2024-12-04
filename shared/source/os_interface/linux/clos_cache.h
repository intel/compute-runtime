/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace NEO {

class IoctlHelper;
enum class CacheLevel : uint16_t;
enum class CacheRegion : uint16_t;

class ClosCacheReservation {
  public:
    ClosCacheReservation(IoctlHelper &ioctlHelper) : ioctlHelper{ioctlHelper} {}

    CacheRegion reserveCache(CacheLevel cacheLevel, uint16_t numWays);
    CacheRegion freeCache(CacheLevel cacheLevel, CacheRegion closIndex);

  protected:
    CacheRegion allocEntry(CacheLevel cacheLevel);
    CacheRegion freeEntry(CacheRegion closIndex);
    uint16_t allocCacheWay(CacheRegion closIndex, CacheLevel cacheLevel, uint16_t numWays);

    IoctlHelper &ioctlHelper;
};

} // namespace NEO
