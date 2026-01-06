/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cpu_copy_helper.h"

#include "shared/source/os_interface/linux/memory_info.h"

#include "drm_neo.h"

namespace NEO {

bool isSmallBarConfigPresent(const OSInterface *osIface) {
    if (osIface == nullptr) {
        return false;
    }

    auto driverModel = osIface->getDriverModel();
    if (driverModel == nullptr) {
        return false;
    }

    if (driverModel->getDriverModelType() != DriverModelType::drm) {
        return false;
    }

    auto drm = static_cast<Drm *>(driverModel);
    auto memInfo = drm->getMemoryInfo();
    if (memInfo == nullptr) {
        return false;
    }

    return memInfo->isSmallBarDetected();

    return false;
}

} // namespace NEO
