/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/cfl/device_ids_configs_cfl.h"

#include "platforms.h"

#include <algorithm>

namespace NEO {
template <>
AOT::PRODUCT_CONFIG HwInfoConfigHw<gfxProduct>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    auto deviceId = hwInfo.platform.usDeviceID;
    bool isCfl = (std::find(cflDeviceIds.begin(), cflDeviceIds.end(), deviceId) != cflDeviceIds.end());
    bool isWhl = (std::find(whlDeviceIds.begin(), whlDeviceIds.end(), deviceId) != whlDeviceIds.end());
    bool isCml = (std::find(cmlDeviceIds.begin(), cmlDeviceIds.end(), deviceId) != cmlDeviceIds.end());

    if (isCfl) {
        return AOT::CFL;
    } else if (isCml) {
        return AOT::CML;
    } else if (isWhl) {
        return AOT::WHL;
    }
    return AOT::UNKNOWN_ISA;
}
} // namespace NEO