/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {

bool OSInterface::osEnabled64kbPages = true;
bool OSInterface::newResourceImplicitFlush = true;
bool OSInterface::gpuIdleImplicitFlush = false;
bool OSInterface::requiresSupportForWddmTrimNotification = true;

bool OSInterface::isDebugAttachAvailable() const {
    if (driverModel) {
        return driverModel->as<NEO::Wddm>()->isDebugAttachAvailable();
    }
    return false;
}

bool OSInterface::isLockablePointer(bool isLockable) const {
    return isLockable;
}

} // namespace NEO
