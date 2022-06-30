/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
uint32_t HwInfoConfigHw<gfxProduct>::getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const {
    switch (stepping) {
    case REVISION_A0:
        return 0x0;
    case REVISION_B:
        return 0x1;
    case REVISION_C:
        return 0x4;
    }
    return CommonConstants::invalidStepping;
}

template <>
AOT::PRODUCT_CONFIG HwInfoConfigHw<gfxProduct>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    return AOT::RKL;
}

template <>
uint32_t HwInfoConfigHw<gfxProduct>::getSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
    switch (hwInfo.platform.usRevId) {
    case 0x0:
        return REVISION_A0;
    case 0x1:
        return REVISION_B;
    case 0x4:
        return REVISION_C;
    }
    return CommonConstants::invalidStepping;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo) const {
    return HwHelper::get(hwInfo.platform.eRenderCoreFamily).isWorkaroundRequired(REVISION_A0, REVISION_C, hwInfo);
}

template <>
bool HwInfoConfigHw<gfxProduct>::is3DPipelineSelectWARequired() const {
    return true;
}
