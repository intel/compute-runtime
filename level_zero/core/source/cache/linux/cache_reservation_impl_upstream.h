/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/cache/cache_reservation.h"

namespace L0 {

class CacheReservationImpl : public CacheReservation {
  public:
    ~CacheReservationImpl() override = default;
    CacheReservationImpl(Device &device){};

    bool reserveCache(size_t cacheLevel, size_t cacheReservationSize) override;
    bool setCacheAdvice(void *ptr, size_t regionSize, ze_cache_ext_region_t cacheRegion) override;
    size_t getMaxCacheReservationSize(size_t cacheLevel) override;
};

} // namespace L0
