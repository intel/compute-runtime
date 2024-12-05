/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/external_semaphore_helper.h"
#include "shared/source/os_interface/windows/external_semaphore_helper_windows.h"

#include <limits>
#include <memory>
#include <string>

namespace NEO {

std::unique_ptr<ExternalSemaphoreHelper> ExternalSemaphoreHelper::create(OSInterface *osInterface) {
    if (osInterface) {
        if (osInterface->getDriverModel()->getDriverModelType() == DriverModelType::wddm) {
            return ExternalSemaphoreHelperWindows::create(osInterface);
        }
    }
    return nullptr;
}

} // namespace NEO