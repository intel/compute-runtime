/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

using namespace NEO;

template <>
uint32_t HwInfoConfigHw<gfxProduct>::getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const {
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
uint32_t HwInfoConfigHw<gfxProduct>::getSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
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
bool HwInfoConfigHw<gfxProduct>::isMaxThreadsForWorkgroupWARequired(const HardwareInfo &hwInfo) const {
    uint32_t stepping = getSteppingFromHwRevId(hwInfo);
    return REVISION_B > stepping;
}

template <>
void HwInfoConfigHw<gfxProduct>::adjustSamplerState(void *sampler, const HardwareInfo &hwInfo) {
    using SAMPLER_STATE = typename XeHpFamily::SAMPLER_STATE;

    auto samplerState = reinterpret_cast<SAMPLER_STATE *>(sampler);
    if (DebugManager.flags.ForceSamplerLowFilteringPrecision.get()) {
        samplerState->setLowQualityFilter(SAMPLER_STATE::LOW_QUALITY_FILTER_ENABLE);
    }
}

template <>
std::string HwInfoConfigHw<gfxProduct>::getDeviceMemoryName() const {
    return "HBM";
}

template <>
bool HwInfoConfigHw<gfxProduct>::isDirectSubmissionSupported(const HardwareInfo &hwInfo) const {
    if (hwInfo.platform.usRevId < HwInfoConfig::get(hwInfo.platform.eProductFamily)->getHwRevIdFromStepping(REVISION_B, hwInfo)) {
        return false;
    }
    return true;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isDisableOverdispatchAvailable(const HardwareInfo &hwInfo) const {
    return getSteppingFromHwRevId(hwInfo) >= REVISION_B;
}

template <>
bool HwInfoConfigHw<gfxProduct>::allowCompression(const HardwareInfo &hwInfo) const {
    if (hwInfo.gtSystemInfo.EUCount == 256u) {
        return false;
    }
    return true;
}

template <>
bool HwInfoConfigHw<gfxProduct>::allowStatelessCompression(const HardwareInfo &hwInfo) const {
    if (!NEO::ApiSpecificConfig::isStatelessCompressionSupported()) {
        return false;
    }
    if (DebugManager.flags.EnableStatelessCompression.get() != -1) {
        return static_cast<bool>(DebugManager.flags.EnableStatelessCompression.get());
    }
    if (!hwInfo.capabilityTable.ftrRenderCompressedBuffers) {
        return false;
    }
    if (HwHelper::getSubDevicesCount(&hwInfo) > 1) {
        return DebugManager.flags.EnableMultiTileCompression.get() > 0 ? true : false;
    }
    if (hwInfo.platform.usRevId < getHwRevIdFromStepping(REVISION_B, hwInfo)) {
        return false;
    }
    return true;
}

template <>
LocalMemoryAccessMode HwInfoConfigHw<gfxProduct>::getDefaultLocalMemoryAccessMode(const HardwareInfo &hwInfo) const {
    if (HwHelper::get(hwInfo.platform.eRenderCoreFamily).isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo)) {
        return LocalMemoryAccessMode::CpuAccessDisallowed;
    }
    return LocalMemoryAccessMode::Default;
}

template <>
std::pair<bool, bool> HwInfoConfigHw<gfxProduct>::isPipeControlPriorToNonPipelinedStateCommandsWARequired(const HardwareInfo &hwInfo, bool isRcs) const {
    auto isBasicWARequired = true;
    auto isExtendedWARequired = hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled > 1;

    if (DebugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.get() != -1) {
        isExtendedWARequired = DebugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.get();
    }

    return {isBasicWARequired, isExtendedWARequired};
}

template <>
bool HwInfoConfigHw<gfxProduct>::heapInLocalMem(const HardwareInfo &hwInfo) const {
    return !HwHelper::get(hwInfo.platform.eDisplayCoreFamily).isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo);
}

template <>
bool HwInfoConfigHw<gfxProduct>::extraParametersInvalid(const HardwareInfo &hwInfo) const {
    return HwHelper::get(hwInfo.platform.eDisplayCoreFamily).isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo);
}

template <>
bool HwInfoConfigHw<gfxProduct>::isBlitterForImagesSupported() const {
    return true;
}
