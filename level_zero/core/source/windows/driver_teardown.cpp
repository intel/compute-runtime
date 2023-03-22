/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/global_teardown.h"
#include "level_zero/tools/source/sysman/os_sysman_driver.h"
#include "level_zero/tools/source/sysman/sysman.h"

#include <windows.h>

namespace L0 {

ze_result_t setDriverTeardownHandleInLoader(std::string loaderLibraryName) {
    HMODULE handle = nullptr;
    ze_result_t result = ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    handle = GetModuleHandleA(loaderLibraryName.c_str());
    if (handle) {
        zelSetDriverTeardown_fn setDriverTeardown = reinterpret_cast<zelSetDriverTeardown_fn>(GetProcAddress(handle, "zelSetDriverTeardown"));
        if (setDriverTeardown) {
            result = setDriverTeardown();
        }
    }
    return result;
}

} // namespace L0

BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_DETACH) {
        std::string loaderLibraryName = L0::loaderLibraryFilename + ".dll";
        L0::setDriverTeardownHandleInLoader(loaderLibraryName);
        L0::globalDriverTeardown();
        if (L0::GlobalOsSysmanDriver != nullptr) {
            delete L0::GlobalOsSysmanDriver;
            L0::GlobalOsSysmanDriver = nullptr;
        }
    }
    return TRUE;
}
