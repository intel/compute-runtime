/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zet_api.h>

#include <memory>
#include <stdint.h>

namespace L0 {

struct Device;

class CacheReservation {
  public:
    virtual ~CacheReservation() = default;

    static std::unique_ptr<CacheReservation> create(Device &device);

    virtual bool reserveCache(size_t cacheLevel, size_t cacheReservationSize) = 0;
    virtual bool setCacheAdvice(void *ptr, size_t regionSize, ze_cache_ext_region_t cacheRegion) = 0;
    virtual size_t getMaxCacheReservationSize(size_t cacheLevel) = 0;
};

} // namespace L0