/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
PRODUCT_CONFIG HwInfoConfigHw<gfxProduct>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    uint32_t stepping = getSteppingFromHwRevId(hwInfo);
    if (stepping == CommonConstants::invalidStepping) {
        return PRODUCT_CONFIG::UNKNOWN_ISA;
    }
    if (DG2::isG10(hwInfo)) {
        switch (stepping) {
        case REVISION_A0:
        case REVISION_A1:
            return PRODUCT_CONFIG::DG2_G10_A0;
        default:
        case REVISION_B:
            return PRODUCT_CONFIG::DG2_G10_B0;
        }
    } else {
        return PRODUCT_CONFIG::DG2_G11;
    }
}