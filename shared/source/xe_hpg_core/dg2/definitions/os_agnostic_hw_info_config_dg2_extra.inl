/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
AOT::PRODUCT_CONFIG HwInfoConfigHw<gfxProduct>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    if (DG2::isG10(hwInfo)) {
        switch (hwInfo.platform.usRevId) {
        case 0x0:
            return AOT::DG2_G10_A0;
        case 0x1:
            return AOT::DG2_G10_A1;
        case 0x4:
            return AOT::DG2_G10_B0;
        case 0x8:
            return AOT::DG2_G10_C0;
        }
    } else if (DG2::isG11(hwInfo)) {
        switch (hwInfo.platform.usRevId) {
        case 0x0:
            return AOT::DG2_G11_A0;
        case 0x4:
            return AOT::DG2_G11_B0;
        case 0x5:
            return AOT::DG2_G11_B1;
        }
    } else if (DG2::isG12(hwInfo)) {
        return AOT::DG2_G12_A0;
    }
    return AOT::UNKNOWN_ISA;
}