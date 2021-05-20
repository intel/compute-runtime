/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/core/source/driver/driver_handle_imp.h"

namespace L0 {

bool comparePciIdBusNumber(std::unique_ptr<NEO::Device> &neoDevice1, std::unique_ptr<NEO::Device> &neoDevice2) {
    // BDF sample format is : 00:02.0
    auto bdfDevice1 = neoDevice1.get()->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Drm>()->getPciPath();
    uint32_t bus1 = 0;
    std::stringstream ss;
    ss << std::hex << bdfDevice1;
    ss >> bus1;
    ss.str("");
    ss.clear();
    auto bdfDevice2 = neoDevice2.get()->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Drm>()->getPciPath();
    uint32_t bus2 = 0;
    ss << std::hex << bdfDevice2;
    ss >> bus2;
    return bus1 < bus2;
}

void DriverHandleImp::sortNeoDevices(std::vector<std::unique_ptr<NEO::Device>> &neoDevices) {
    std::sort(neoDevices.begin(), neoDevices.end(), comparePciIdBusNumber);
}

} // namespace L0
