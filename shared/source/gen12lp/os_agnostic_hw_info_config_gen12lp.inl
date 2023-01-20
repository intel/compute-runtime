/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {
template <>
void ProductHelperHw<gfxProduct>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {
    hwInfo->platform.eRenderCoreFamily = GFXCORE_FAMILY::IGFX_GEN12LP_CORE;
    hwInfo->platform.eDisplayCoreFamily = GFXCORE_FAMILY::IGFX_GEN12LP_CORE;
}

template <>
bool ProductHelperHw<gfxProduct>::isPageTableManagerSupported(const HardwareInfo &hwInfo) const {
    return hwInfo.capabilityTable.ftrRenderCompressedBuffers || hwInfo.capabilityTable.ftrRenderCompressedImages;
}

template <>
void ProductHelperHw<gfxProduct>::adjustSamplerState(void *sampler, const HardwareInfo &hwInfo) const {
    using SAMPLER_STATE = typename Gen12LpFamily::SAMPLER_STATE;
    auto samplerState = reinterpret_cast<SAMPLER_STATE *>(sampler);
    if (DebugManager.flags.ForceSamplerLowFilteringPrecision.get()) {
        samplerState->setLowQualityFilter(SAMPLER_STATE::LOW_QUALITY_FILTER_ENABLE);
    }
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getMaxThreadsForWorkgroupInDSSOrSS(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice, uint32_t maxNumEUsPerDualSubSlice) const {
    return getMaxThreadsForWorkgroup(hwInfo, maxNumEUsPerDualSubSlice);
}
} // namespace NEO
