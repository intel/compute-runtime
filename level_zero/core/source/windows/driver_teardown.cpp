/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/windows_wrapper.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/global_teardown.h"
#include "level_zero/tools/source/sysman/os_sysman_driver.h"
#include "level_zero/tools/source/sysman/sysman.h"

/*
 * DllMain is called by the OS when the DLL is loaded or unloaded.
 * When modifying the code here, be aware of the restrictions on what can be done
 * inside DllMain. See https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices
 * for more information.
 */
BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) { // NOLINT(readability-identifier-naming)
    if (fdwReason == DLL_PROCESS_ATTACH) {
        L0::globalDriverSetup();
    }
    if (fdwReason == DLL_PROCESS_DETACH) {
        L0::globalDriverTeardown();
        if (L0::globalOsSysmanDriver != nullptr) {
            delete L0::globalOsSysmanDriver;
            L0::globalOsSysmanDriver = nullptr;
        }
    }
    return TRUE;
}
