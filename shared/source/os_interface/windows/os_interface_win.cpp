/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
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

bool OSInterface::isSizeWithinThresholdForStaging(const void *ptr, size_t size) const {
    return true;
}

uint32_t OSInterface::getAggregatedProcessCount() const {
    return 0;
}

void OSInterface::registerTrimCallback() {
    if (driverModel && driverModel->getDriverModelType() == DriverModelType::wddm) {

        if (NEO::debugManager.flags.NEO_L0_SYSMAN_NO_CONTEXT_MODE.get()) {
            return;
        }

        driverModel->as<NEO::Wddm>()->getResidencyController().registerCallback();
        UNRECOVERABLE_IF(!driverModel->as<NEO::Wddm>()->getResidencyController().isInitialized());
    }
}

void OSInterface::unregisterTrimCallback() {
    if (driverModel && driverModel->getDriverModelType() == DriverModelType::wddm) {
        driverModel->unregisterTrimCallback();
    }
}

} // namespace NEO
