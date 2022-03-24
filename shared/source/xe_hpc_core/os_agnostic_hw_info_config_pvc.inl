/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
bool HwInfoConfigHw<gfxProduct>::isMaxThreadsForWorkgroupWARequired(const HardwareInfo &hwInfo) const {
    return PVC::isXl(hwInfo);
}

template <>
uint32_t HwInfoConfigHw<gfxProduct>::getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const {
    if (PVC::isXt(hwInfo)) {
        switch (stepping) {
        case REVISION_A0:
            return 0x3;
        case REVISION_B:
            return 0x9D;
        case REVISION_C:
            return 0x7;
        }
    } else {
        switch (stepping) {
        case REVISION_A0:
            return 0x0;
        case REVISION_B:
            return 0x6;
        case REVISION_C:
            DEBUG_BREAK_IF(true);
            return 0x7;
        }
    }

    return CommonConstants::invalidStepping;
}

template <>
uint32_t HwInfoConfigHw<gfxProduct>::getSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
    switch (hwInfo.platform.usRevId & PVC::pvcSteppingBits) {
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
uint32_t HwInfoConfigHw<gfxProduct>::getDeviceMemoryMaxClkRate(const HardwareInfo *hwInfo) {
    bool isBaseDieA0 = (hwInfo->platform.usRevId & XE_HPC_COREFamily::pvcBaseDieRevMask) == XE_HPC_COREFamily::pvcBaseDieA0Masked;
    if (isBaseDieA0) {
        // For IGFX_PVC REV A0 HBM frequency would be 3.2 GT/s = 3.2 * 1000 MT/s = 3200 MT/s
        return 3200u;
    }
    return 0u;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isDisableOverdispatchAvailable(const HardwareInfo &hwInfo) const {
    return getSteppingFromHwRevId(hwInfo) >= REVISION_B;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isSpecialPipelineSelectModeChanged(const HardwareInfo &hwInfo) const {
    return PVC::isAtMostXtA0(hwInfo);
}

template <>
bool HwInfoConfigHw<gfxProduct>::isSystolicModeConfigurable(const HardwareInfo &hwInfo) const {
    return PVC::isAtMostXtA0(hwInfo);
}

template <>
bool HwInfoConfigHw<gfxProduct>::isGlobalFenceInCommandStreamRequired(const HardwareInfo &hwInfo) const {
    return !PVC::isXlA0(hwInfo);
}

template <>
bool HwInfoConfigHw<gfxProduct>::isAdjustProgrammableIdPreferredSlmSizeRequired(const HardwareInfo &hwInfo) const {
    return PVC::isXlA0(hwInfo);
}
