/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace NEO {

class Drm;
enum class CacheLevel : uint16_t;
enum class CacheRegion : uint16_t;

class ClosCacheReservation {
  public:
    ClosCacheReservation(Drm &drm) : drm(drm) {}

    CacheRegion reserveCache(CacheLevel cacheLevel, uint16_t numWays);
    CacheRegion freeCache(CacheLevel cacheLevel, CacheRegion closIndex);

  protected:
    CacheRegion allocEntry();
    CacheRegion freeEntry(CacheRegion closIndex);
    uint16_t allocCacheWay(CacheRegion closIndex, CacheLevel cacheLevel, uint16_t numWays);

    Drm &drm;
};

} // namespace NEO
