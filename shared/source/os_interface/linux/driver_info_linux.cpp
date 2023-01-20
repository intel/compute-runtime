/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/driver_info_linux.h"

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_interface.h"

namespace NEO {

DriverInfo *DriverInfo::create(const HardwareInfo *hwInfo, const OSInterface *osInterface) {
    PhysicalDevicePciBusInfo pciBusInfo(PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue);
    if (osInterface) {
        pciBusInfo = osInterface->getDriverModel()->getPciBusInfo();
    }
    if (hwInfo) {
        auto imageSupport = hwInfo->capabilityTable.supportsImages;
        return new DriverInfoLinux(imageSupport, pciBusInfo);
    }
    return nullptr;
};

DriverInfoLinux::DriverInfoLinux(bool imageSupport, const PhysicalDevicePciBusInfo &pciBusInfo)
    : DriverInfo(DriverInfoType::LINUX), imageSupport(imageSupport) {
    this->pciBusInfo = pciBusInfo;
}

bool DriverInfoLinux::getMediaSharingSupport() { return imageSupport; }

} // namespace NEO
