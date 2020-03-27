/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/driver_info_linux.h"

#include "shared/source/helpers/hw_info.h"

namespace NEO {

DriverInfo *DriverInfo::create(const HardwareInfo *hwInfo, OSInterface *osInterface) {
    if (hwInfo) {
        auto imageSupport = hwInfo->capabilityTable.supportsImages;
        return new DriverInfoLinux(imageSupport);
    }
    return nullptr;
};

DriverInfoLinux::DriverInfoLinux(bool imageSupport) : imageSupport(imageSupport) {}

bool DriverInfoLinux::getImageSupport() { return imageSupport; }

} // namespace NEO
