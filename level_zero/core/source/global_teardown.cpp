/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/global_teardown.h"

#include "shared/source/os_interface/os_library.h"

#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/sysman/source/sysman_driver_handle_imp.h"

namespace L0 {

ze_result_t loaderDriverTeardown(std::string loaderLibraryName) {
    std::unique_ptr<NEO::OsLibrary> loaderLibrary = std::unique_ptr<NEO::OsLibrary>{NEO::OsLibrary::load(loaderLibraryName.c_str())};
    if (loaderLibrary) {
        zelSetDriverTeardown_fn setDriverTeardown = reinterpret_cast<zelSetDriverTeardown_fn>(loaderLibrary->getProcAddress("zelSetDriverTeardown"));
        if (setDriverTeardown) {
            return setDriverTeardown();
        }
    }
    return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
}

void globalDriverTeardown() {
    if (GlobalDriver != nullptr) {
        delete GlobalDriver;
        GlobalDriver = nullptr;
    }
    if (Sysman::GlobalSysmanDriver != nullptr) {
        delete Sysman::GlobalSysmanDriver;
        Sysman::GlobalSysmanDriver = nullptr;
    }
}
} // namespace L0
