/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/product_helper.h"

namespace NEO {
template <>
int ProductHelperHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const {
    enableCompression(hwInfo);

    hwInfo->featureTable.flags.ftr57bGPUAddressing = (hwInfo->capabilityTable.gpuAddressSpace == maxNBitValue(57));

    enableBlitterOperationsSupport(hwInfo);

    auto &kmdNotifyProperties = hwInfo->capabilityTable.kmdNotifyProperties;
    kmdNotifyProperties.enableKmdNotify = true;
    kmdNotifyProperties.delayKmdNotifyMicroseconds = 150;
    kmdNotifyProperties.enableQuickKmdSleepForDirectSubmission = true;
    kmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds = 20;

    return 0;
}

template <>
bool ProductHelperHw<gfxProduct>::isNonBlockingGpuSubmissionSupported() const {
    return false;
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getNumCacheRegions() const {
    constexpr uint32_t numSharedCacheRegions = 1;
    constexpr uint32_t numReservedCacheRegions = 2;
    constexpr uint32_t numTotalCacheRegions = numSharedCacheRegions + numReservedCacheRegions;
    return numTotalCacheRegions;
}

template <>
uint64_t ProductHelperHw<gfxProduct>::getPatIndex(CacheRegion cacheRegion, CachePolicy cachePolicy) const {
    /*
    PAT Index  CLOS   MemType
    SHARED
    0          0      UC (00)
    1          0      WC (01)
    2          0      WT (10)
    3          0      WB (11)
    RESERVED 1
    4          1      WT (10)
    5          1      WB (11)
    RESERVED 2
    6          2      WT (10)
    7          2      WB (11)
    */

    if ((debugManager.flags.ForceAllResourcesUncached.get() == true)) {
        cacheRegion = CacheRegion::defaultRegion;
        cachePolicy = CachePolicy::uncached;
    }

    UNRECOVERABLE_IF((cacheRegion > CacheRegion::defaultRegion) && (cachePolicy < CachePolicy::writeThrough));
    return (static_cast<uint32_t>(cachePolicy) + (static_cast<uint16_t>(cacheRegion) * 2));
}

} // namespace NEO
