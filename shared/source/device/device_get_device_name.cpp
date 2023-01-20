/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_info.h"

#include <iomanip>
#include <sstream>
#include <string>

namespace NEO {

const std::string Device::getDeviceName() const {
    auto &hwInfo = this->getHardwareInfo();
    std::string deviceName = hwInfo.capabilityTable.deviceName;
    if (!deviceName.empty()) {
        return deviceName;
    }

    std::stringstream deviceNameDefault;
    deviceNameDefault << "Intel(R) Graphics";
    deviceNameDefault << " [0x" << std::hex << std::setw(4) << std::setfill('0') << hwInfo.platform.usDeviceID << "]";

    return deviceNameDefault.str();
}
} // namespace NEO
