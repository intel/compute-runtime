/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cache/windows/cache_reservation_impl.h"

namespace L0 {

std::unique_ptr<CacheReservation> CacheReservation::create(Device &device) {
    return std::make_unique<CacheReservationImpl>(device);
}

bool CacheReservationImpl::reserveCache(size_t cacheLevel, size_t cacheReservationSize) {
    return false;
}

bool CacheReservationImpl::setCacheAdvice(void *ptr, size_t regionSize, ze_cache_ext_region_t cacheRegion) {
    return false;
}

size_t CacheReservationImpl::getMaxCacheReservationSize() {
    return 0;
}

} // namespace L0
