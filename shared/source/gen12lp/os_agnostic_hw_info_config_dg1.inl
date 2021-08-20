/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
uint32_t HwInfoConfigHw<gfxProduct>::getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const {
    if (hwInfo.platform.eProductFamily == gfxProduct) {
        switch (stepping) {
        case REVISION_A0:
            return 0x0;
        case REVISION_B:
            return 0x1;
        }
    }
    return CommonConstants::invalidStepping;
}
