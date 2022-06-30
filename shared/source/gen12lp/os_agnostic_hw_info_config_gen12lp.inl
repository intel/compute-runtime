/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void HwInfoConfigHw<gfxProduct>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {
    hwInfo->platform.eRenderCoreFamily = GFXCORE_FAMILY::IGFX_GEN12LP_CORE;
    hwInfo->platform.eDisplayCoreFamily = GFXCORE_FAMILY::IGFX_GEN12LP_CORE;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isPageTableManagerSupported(const HardwareInfo &hwInfo) const {
    return hwInfo.capabilityTable.ftrRenderCompressedBuffers || hwInfo.capabilityTable.ftrRenderCompressedImages;
}

template <>
void HwInfoConfigHw<gfxProduct>::adjustSamplerState(void *sampler, const HardwareInfo &hwInfo) {
    using SAMPLER_STATE = typename TGLLPFamily::SAMPLER_STATE;
    auto samplerState = reinterpret_cast<SAMPLER_STATE *>(sampler);
    if (DebugManager.flags.ForceSamplerLowFilteringPrecision.get()) {
        samplerState->setLowQualityFilter(SAMPLER_STATE::LOW_QUALITY_FILTER_ENABLE);
    }
}