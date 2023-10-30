/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/windows_wrapper.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/global_teardown.h"
#include "level_zero/tools/source/sysman/os_sysman_driver.h"
#include "level_zero/tools/source/sysman/sysman.h"

namespace L0 {

ze_result_t setDriverTeardownHandleInLoader(const char *loaderLibraryName) {
    if (L0::LevelZeroDriverInitialized) {
        HMODULE handle = nullptr;
        ze_result_t result = ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
        handle = GetModuleHandleA(loaderLibraryName);
        if (handle) {
            zelSetDriverTeardown_fn setDriverTeardown = reinterpret_cast<zelSetDriverTeardown_fn>(GetProcAddress(handle, "zelSetDriverTeardown"));
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

BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) { // NOLINT(readability-identifier-naming)
    if (fdwReason == DLL_PROCESS_DETACH) {
        L0::setDriverTeardownHandleInLoader("ze_loader.dll");
        L0::globalDriverTeardown();
        if (L0::GlobalOsSysmanDriver != nullptr) {
            delete L0::GlobalOsSysmanDriver;
            L0::GlobalOsSysmanDriver = nullptr;
        }
    }
    return TRUE;
}
