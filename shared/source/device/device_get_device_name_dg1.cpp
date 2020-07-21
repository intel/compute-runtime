/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {

const std::string Device::getDeviceName(const HardwareInfo &hwInfo) const {
    std::string deviceName = "Intel(R) Graphics";
    if (hwInfo.platform.eProductFamily < PRODUCT_FAMILY::IGFX_DG1) {
        deviceName += std::string(" ") + familyName[hwInfo.platform.eRenderCoreFamily];
    }
    return deviceName;
}
} // namespace NEO
