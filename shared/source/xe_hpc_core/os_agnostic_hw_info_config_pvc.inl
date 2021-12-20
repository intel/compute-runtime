/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void HwInfoConfigHw<gfxProduct>::getKernelExtendedProperties(uint32_t *fp16, uint32_t *fp32, uint32_t *fp64) {
    *fp16 = (FP_ATOMIC_EXT_FLAG_GLOBAL_LOAD_STORE | FP_ATOMIC_EXT_FLAG_GLOBAL_ADD | FP_ATOMIC_EXT_FLAG_GLOBAL_MIN_MAX);
    *fp32 = (FP_ATOMIC_EXT_FLAG_GLOBAL_LOAD_STORE | FP_ATOMIC_EXT_FLAG_GLOBAL_ADD | FP_ATOMIC_EXT_FLAG_GLOBAL_MIN_MAX);
    *fp64 = (FP_ATOMIC_EXT_FLAG_GLOBAL_LOAD_STORE | FP_ATOMIC_EXT_FLAG_GLOBAL_ADD | FP_ATOMIC_EXT_FLAG_GLOBAL_MIN_MAX);
}

template <>
bool HwInfoConfigHw<gfxProduct>::isMaxThreadsForWorkgroupWARequired(const HardwareInfo &hwInfo) const {
    auto deviceId = hwInfo.platform.usDeviceID;
    return XE_HPC_COREFamily::pvcXlDeviceId == deviceId;
}

template <>
uint32_t HwInfoConfigHw<gfxProduct>::getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const {
    switch (hwInfo.platform.usDeviceID) {
    default:
    case XE_HPC_COREFamily::pvcXtTemporaryDeviceId:
    case XE_HPC_COREFamily::pvcXlDeviceId:
        switch (stepping) {
        case REVISION_A0:
            return 0x0;
        case REVISION_B:
            return 0x6;
        case REVISION_C:
            DEBUG_BREAK_IF(true);
            return 0x7;
        }
        break;
    case XE_HPC_COREFamily::pvcXtDeviceIds[0]:
    case XE_HPC_COREFamily::pvcXtDeviceIds[1]:
    case XE_HPC_COREFamily::pvcXtDeviceIds[2]:
        switch (stepping) {
        case REVISION_A0:
            return 0x3;
        case REVISION_B:
            return 0x9D;
        case REVISION_C:
            return 0x7;
        }
    }
    return CommonConstants::invalidStepping;
}

template <>
uint32_t HwInfoConfigHw<gfxProduct>::getSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
    switch (hwInfo.platform.usRevId & XE_HPC_COREFamily::pvcSteppingBits) {
    case 0x0:
    case 0x1:
    case 0x3:
        return REVISION_A0;
    case 0x5:
    case 0x6:
        return REVISION_B;
    case 0x7:
        return REVISION_C;
    }
    return CommonConstants::invalidStepping;
}

template <>
void HwInfoConfigHw<gfxProduct>::adjustSamplerState(void *sampler, const HardwareInfo &hwInfo) {
    using SAMPLER_STATE = typename XE_HPC_COREFamily::SAMPLER_STATE;

    auto samplerState = reinterpret_cast<SAMPLER_STATE *>(sampler);
    if (DebugManager.flags.ForceSamplerLowFilteringPrecision.get()) {
        samplerState->setLowQualityFilter(SAMPLER_STATE::LOW_QUALITY_FILTER_ENABLE);
    }
}

template <>
uint32_t HwInfoConfigHw<gfxProduct>::getDeviceMemoryMaxClkRate(const HardwareInfo *hwInfo) {
    bool isBaseDieA0 = (hwInfo->platform.usRevId & XE_HPC_COREFamily::pvcBaseDieRevMask) == XE_HPC_COREFamily::pvcBaseDieA0Masked;
    if (isBaseDieA0) {
        // For IGFX_PVC REV A0 HBM frequency would be 3.2 GT/s = 3.2 * 1000 MT/s = 3200 MT/s
        return 3200u;
    }
    return 0u;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isDirectSubmissionSupported(const HardwareInfo &hwInfo) const {
    return true;
}

template <>
std::string HwInfoConfigHw<gfxProduct>::getDeviceMemoryName() const {
    return "HBM";
}

template <>
bool HwInfoConfigHw<gfxProduct>::isDisableOverdispatchAvailable(const HardwareInfo &hwInfo) const {
    return getSteppingFromHwRevId(hwInfo) >= REVISION_B;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isPipeControlPriorToNonPipelinedStateCommandsWARequired(const HardwareInfo &hwInfo, bool isRcs) const {
    bool required = hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled > 1;

    if (DebugManager.flags.ProgramPipeControlPriorToNonPipelinedStateCommand.get() != -1) {
        required = DebugManager.flags.ProgramPipeControlPriorToNonPipelinedStateCommand.get();
    }

    return required;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isDcFlushAllowed() const {
    return false;
}
