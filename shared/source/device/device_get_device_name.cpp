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
    return std::string(hwInfo.capabilityTable.deviceName).empty() ? "Intel(R) Graphics" : hwInfo.capabilityTable.deviceName;
}
} // namespace NEO
