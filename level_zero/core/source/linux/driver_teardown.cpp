/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_library.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/global_teardown.h"

#include <memory>

namespace L0 {

ze_result_t setDriverTeardownHandleInLoader(std::string loaderLibraryName) {
    if (L0::LevelZeroDriverInitialized) {
        ze_result_t result = ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
        std::unique_ptr<NEO::OsLibrary> loaderLibrary = std::unique_ptr<NEO::OsLibrary>{NEO::OsLibrary::load(loaderLibraryName.c_str())};
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
    std::string loaderLibraryName = "lib" + L0::loaderLibraryFilename + ".so.1";
    L0::setDriverTeardownHandleInLoader(loaderLibraryName);
    L0::globalDriverTeardown();
}