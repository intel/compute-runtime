/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/driver_info_windows.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {

DriverInfo *DriverInfo::create(const HardwareInfo *hwInfo, const OSInterface *osInterface) {
    if (osInterface == nullptr) {
        return nullptr;
    }

    auto wddm = osInterface->getDriverModel()->as<Wddm>();
    UNRECOVERABLE_IF(wddm == nullptr);

    return new DriverInfoWindows(wddm->getDeviceRegistryPath(), wddm->getPciBusInfo());
};

} // namespace NEO