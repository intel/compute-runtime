/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/external_semaphore.h"
#include "shared/source/os_interface/windows/external_semaphore_windows.h"

#include <limits>
#include <memory>
#include <string>

namespace NEO {

std::unique_ptr<ExternalSemaphore> ExternalSemaphore::create(OSInterface *osInterface, ExternalSemaphore::Type type, void *handle, int fd) {
    if (osInterface) {
        if (osInterface->getDriverModel()->getDriverModelType() == DriverModelType::wddm) {
            auto externalSemaphore = ExternalSemaphoreWindows::create(osInterface);

            bool result = externalSemaphore->importSemaphore(handle, fd, 0, nullptr, type, false);
            if (result == false) {
                return nullptr;
            }

            return externalSemaphore;
        }
    }
    return nullptr;
}

} // namespace NEO
