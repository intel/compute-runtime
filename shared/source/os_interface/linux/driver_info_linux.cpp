/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/driver_info_linux.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/os_interface.h"

namespace NEO {

DriverInfo *DriverInfo::create(const HardwareInfo *hwInfo, const OSInterface *osInterface) {
    PhysicalDevicePciBusInfo pciBusInfo(PhysicalDevicePciBusInfo::InvalidValue, PhysicalDevicePciBusInfo::InvalidValue, PhysicalDevicePciBusInfo::InvalidValue, PhysicalDevicePciBusInfo::InvalidValue);
    if (osInterface) {
        auto osInterfaceImpl = osInterface->get();
        UNRECOVERABLE_IF(osInterfaceImpl == nullptr);

        auto drm = osInterface->get()->getDrm();
        UNRECOVERABLE_IF(drm == nullptr);

        pciBusInfo = drm->getPciBusInfo();
    }
    if (hwInfo) {
        auto imageSupport = hwInfo->capabilityTable.supportsImages;
        return new DriverInfoLinux(imageSupport, pciBusInfo);
    }
    return nullptr;
};

DriverInfoLinux::DriverInfoLinux(bool imageSupport, const PhysicalDevicePciBusInfo &pciBusInfo)
    : imageSupport(imageSupport) {
    this->pciBusInfo = pciBusInfo;
}

bool DriverInfoLinux::getImageSupport() { return imageSupport; }

} // namespace NEO
