/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/global_teardown.h"

#include "shared/source/os_interface/sys_calls_common.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"

namespace L0 {

decltype(&zelLoaderTranslateHandle) loaderTranslateHandleFunc = nullptr;
decltype(&zelSetDriverTeardown) setDriverTeardownFunc = nullptr;

void globalDriverSetup() {
    std::unique_ptr<NEO::OsLibrary> loaderLibrary = std::unique_ptr<NEO::OsLibrary>{NEO::OsLibrary::loadFunc("")};
    loaderTranslateHandleFunc = reinterpret_cast<decltype(&zelLoaderTranslateHandle)>(loaderLibrary->getProcAddress("zelLoaderTranslateHandle"));
    setDriverTeardownFunc = reinterpret_cast<decltype(&zelSetDriverTeardown)>(loaderLibrary->getProcAddress("zelSetDriverTeardown"));
}

void globalDriverTeardown() {
    if (levelZeroDriverInitialized && setDriverTeardownFunc) {
        setDriverTeardownFunc();
    }
    if (globalDriver != nullptr) {

        if (globalDriver->pid == NEO::SysCalls::getCurrentProcessId()) {
            delete globalDriver;
        }
        globalDriver = nullptr;
    }
    if (Sysman::globalSysmanDriver != nullptr) {
        delete Sysman::globalSysmanDriver;
        Sysman::globalSysmanDriver = nullptr;
    }
}
} // namespace L0
