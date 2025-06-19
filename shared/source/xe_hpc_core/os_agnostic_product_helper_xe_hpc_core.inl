/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper_from_xe_hpc_to_xe3.inl"
#include "shared/source/os_interface/product_helper_from_xe_hpg_to_xe3.inl"
#include "shared/source/os_interface/product_helper_xe_hpc_and_later.inl"

namespace NEO {

template <>
bool ProductHelperHw<gfxProduct>::isDirectSubmissionSupported(ReleaseHelper *releaseHelper) const {
    return true;
}

template <>
void ProductHelperHw<gfxProduct>::adjustSamplerState(void *sampler, const HardwareInfo &hwInfo) const {
    using SAMPLER_STATE = typename XeHpcCoreFamily::SAMPLER_STATE;

    auto samplerState = reinterpret_cast<SAMPLER_STATE *>(sampler);
    if (debugManager.flags.ForceSamplerLowFilteringPrecision.get()) {
        samplerState->setLowQualityFilter(SAMPLER_STATE::LOW_QUALITY_FILTER_ENABLE);
    }
}

template <>
bool ProductHelperHw<gfxProduct>::isPrefetcherDisablingInDirectSubmissionRequired() const {
    return false;
}

template <>
bool ProductHelperHw<gfxProduct>::isLinearStoragePreferred(bool isImage1d, bool forceLinearStorage) const {
    return true;
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getMaxNumSamplers() const {
    return 0u;
}

template <>
bool ProductHelperHw<gfxProduct>::isDeviceToHostCopySignalingFenceRequired() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isDeviceUsmAllocationReuseSupported() const {
    return false;
}

template <>
bool ProductHelperHw<gfxProduct>::isHostUsmAllocationReuseSupported() const {
    return false;
}

template <>
bool ProductHelperHw<gfxProduct>::isBufferPoolAllocatorSupported() const {
    return false;
}

template <>
bool ProductHelperHw<gfxProduct>::isHostUsmPoolAllocatorSupported() const {
    return false;
}

template <>
bool ProductHelperHw<gfxProduct>::isDeviceUsmPoolAllocatorSupported() const {
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
