/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cache/linux/cache_reservation_impl_prelim.h"

#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/linux/cache_info.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_neo.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"

namespace L0 {

std::unique_ptr<CacheReservation> CacheReservation::create(Device &device) {
    return std::make_unique<CacheReservationImpl>(device);
}

bool CacheReservationImpl::reserveCache(size_t cacheLevel, size_t cacheReservationSize) {
    auto drm = device.getOsInterface().getDriverModel()->as<NEO::Drm>();

    auto cacheInfo = drm->getL3CacheInfo();

    if (cacheReservationSize == 0) {
        cacheInfo->freeCacheRegion(this->reservedL3CacheRegion);
        this->reservedL3CacheRegion = NEO::CacheRegion::none;
        this->reservedL3CacheSize = 0;
        return true;
    }

    auto cacheRegion = cacheInfo->reserveCacheRegion(cacheReservationSize);
    if (cacheRegion == NEO::CacheRegion::none) {
        return false;
    }

    this->reservedL3CacheRegion = cacheRegion;
    this->reservedL3CacheSize = cacheReservationSize;

    return true;
}

bool CacheReservationImpl::setCacheAdvice(void *ptr, [[maybe_unused]] size_t regionSize, ze_cache_ext_region_t cacheRegion) {
    auto cacheRegionIdx = NEO::CacheRegion::defaultRegion;
    size_t cacheRegionSize = 0;

    if (cacheRegion == ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_ZE_CACHE_RESERVE_REGION) {
        cacheRegionIdx = this->reservedL3CacheRegion;
        cacheRegionSize = this->reservedL3CacheSize;
    }

    auto allocData = device.getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    if (allocData == nullptr) {
        return false;
    }

    auto drm = device.getOsInterface().getDriverModel()->as<NEO::Drm>();

    auto gpuAllocation = allocData->gpuAllocations.getGraphicsAllocation(device.getRootDeviceIndex());
    auto drmAllocation = static_cast<NEO::DrmAllocation *>(gpuAllocation);

    return drmAllocation->setCacheAdvice(drm, cacheRegionSize, cacheRegionIdx, !drmAllocation->isAllocatedInLocalMemoryPool());
}

size_t CacheReservationImpl::getMaxCacheReservationSize() {
    auto drm = device.getOsInterface().getDriverModel()->as<NEO::Drm>();

    auto cacheInfo = drm->getL3CacheInfo();
    return cacheInfo->getMaxReservationCacheSize();
}

} // namespace L0
