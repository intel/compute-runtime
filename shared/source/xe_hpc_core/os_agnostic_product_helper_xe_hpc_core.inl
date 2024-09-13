/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper_pvc_and_later.inl"

namespace NEO {
template <>
std::string ProductHelperHw<gfxProduct>::getDeviceMemoryName() const {
    return "HBM";
}

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

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isBufferPoolAllocatorSupported() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isUsmPoolAllocatorSupported() const {
    return false;
}

} // namespace NEO
