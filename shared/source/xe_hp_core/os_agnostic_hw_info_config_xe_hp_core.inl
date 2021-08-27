/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

using namespace NEO;

template <>
bool HwInfoConfigHw<IGFX_XE_HP_SDV>::isMaxThreadsForWorkgroupWARequired(const HardwareInfo &hwInfo) const {
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    uint32_t stepping = hwInfoConfig.getSteppingFromHwRevId(hwInfo);
    return REVISION_B > stepping;
}

template <>
uint32_t HwInfoConfigHw<IGFX_XE_HP_SDV>::getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const {
    switch (stepping) {
    case REVISION_A0:
        return 0x0;
    case REVISION_A1:
        return 0x1;
    case REVISION_B:
        return 0x4;
    }
    return CommonConstants::invalidStepping;
}

template <>
uint32_t HwInfoConfigHw<IGFX_XE_HP_SDV>::getSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
    switch (hwInfo.platform.usRevId) {
    case 0x0:
        return REVISION_A0;
    case 0x1:
        return REVISION_A1;
    case 0x4:
        return REVISION_B;
    }
    return CommonConstants::invalidStepping;
}

template <>
void HwInfoConfigHw<IGFX_XE_HP_SDV>::adjustSamplerState(void *sampler, const HardwareInfo &hwInfo) {
    using SAMPLER_STATE = typename XeHpFamily::SAMPLER_STATE;

    auto samplerState = reinterpret_cast<SAMPLER_STATE *>(sampler);
    if (DebugManager.flags.ForceSamplerLowFilteringPrecision.get()) {
        samplerState->setLowQualityFilter(SAMPLER_STATE::LOW_QUALITY_FILTER_ENABLE);
    }
}