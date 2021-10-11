/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_interface.h"

namespace NEO {

std::vector<std::unique_ptr<HwDeviceId>> OSInterface::discoverDevices(ExecutionEnvironment &executionEnvironment) {
    return Drm::discoverDevices(executionEnvironment);
}

std::vector<std::unique_ptr<HwDeviceId>> OSInterface::discoverDevice(ExecutionEnvironment &executionEnvironment, std::string &osPciPath) {
    return Drm::discoverDevice(executionEnvironment, osPciPath);
}

} // namespace NEO
