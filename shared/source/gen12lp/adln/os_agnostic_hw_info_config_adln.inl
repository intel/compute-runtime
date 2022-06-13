/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
uint32_t HwInfoConfigHw<gfxProduct>::getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const {
    switch (stepping) {
    case REVISION_A0:
        return 0x0;
    }
    return CommonConstants::invalidStepping;
}

template <>
AOT::PRODUCT_CONFIG HwInfoConfigHw<gfxProduct>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    return AOT::ADL_N;
}

template <>
uint32_t HwInfoConfigHw<gfxProduct>::getSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
    switch (hwInfo.platform.usRevId) {
    case 0x0:
        return REVISION_A0;
    }
    return CommonConstants::invalidStepping;
}
