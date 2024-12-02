/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/common_types.h"
#include "shared/source/os_interface/linux/cache_info.h"

#include "level_zero/core/source/cache/cache_reservation.h"

namespace L0 {

class CacheReservationImpl : public CacheReservation {
  public:
    ~CacheReservationImpl() override = default;
    CacheReservationImpl(Device &device) : device(device){};

    bool reserveCache(size_t cacheLevel, size_t cacheReservationSize) override;
    bool setCacheAdvice(void *ptr, size_t regionSize, ze_cache_ext_region_t cacheRegion) override;
    size_t getMaxCacheReservationSize() override;

  protected:
    Device &device;
    NEO::CacheRegion reservedL3CacheRegion = NEO::CacheRegion::none;
    size_t reservedL3CacheSize = 0;
};

} // namespace L0
