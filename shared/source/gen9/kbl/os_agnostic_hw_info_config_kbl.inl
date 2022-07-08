/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/kbl/device_ids_configs_kbl.h"

#include "platforms.h"

#include <algorithm>

namespace NEO {
template <>
AOT::PRODUCT_CONFIG HwInfoConfigHw<gfxProduct>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    auto deviceId = hwInfo.platform.usDeviceID;
    bool isKbl = (std::find(kblDeviceIds.begin(), kblDeviceIds.end(), deviceId) != kblDeviceIds.end());
    bool isAml = (std::find(amlDeviceIds.begin(), amlDeviceIds.end(), deviceId) != amlDeviceIds.end());

    if (isKbl) {
        return AOT::KBL;
    } else if (isAml) {
        return AOT::AML;
    }
    return AOT::UNKNOWN_ISA;
}
} // namespace NEO