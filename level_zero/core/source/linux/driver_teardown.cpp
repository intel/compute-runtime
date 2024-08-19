/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_library.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/global_teardown.h"

#include <memory>

namespace L0 {

ze_result_t setDriverTeardownHandleInLoader(const char *loaderLibraryName) {
    if (L0::levelZeroDriverInitialized) {
        ze_result_t result = ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
        std::unique_ptr<NEO::OsLibrary> loaderLibrary = std::unique_ptr<NEO::OsLibrary>{NEO::OsLibrary::loadFunc(loaderLibraryName)};
        if (loaderLibrary) {
            zelSetDriverTeardown_fn setDriverTeardown = reinterpret_cast<zelSetDriverTeardown_fn>(loaderLibrary->getProcAddress("zelSetDriverTeardown"));
            if (setDriverTeardown) {
                result = setDriverTeardown();
            }
        }
        return result;
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

} // namespace L0

void __attribute__((destructor)) driverHandleDestructor() {
    L0::setDriverTeardownHandleInLoader("libze_loader.so.1");
    L0::globalDriverTeardown();
}
