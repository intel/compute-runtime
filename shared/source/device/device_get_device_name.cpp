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
    std::string deviceName = "Intel(R) Graphics ";
    deviceName += familyName[hwInfo.platform.eRenderCoreFamily];
    return deviceName;
}
} // namespace NEO
