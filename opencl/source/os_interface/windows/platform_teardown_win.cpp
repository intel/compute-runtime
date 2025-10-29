/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/windows_wrapper.h"

#include "opencl/source/global_teardown/global_platform_teardown.h"

/*
 * DllMain is called by the OS when the DLL is loaded or unloaded.
 * When modifying the code here, be aware of the restrictions on what can be done
 * inside DllMain. See https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices
 * for more information.
 */
using namespace NEO;

BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) { // NOLINT(readability-identifier-naming)
    if (fdwReason == DLL_PROCESS_DETACH) {
        /* If lpvReserved is non-NULL with DLL_PROCESS_DETACH, the process is terminating,
         * clean up should be skipped according to the DllMain spec. */
        globalPlatformTeardown(lpvReserved != nullptr);
    }
    if (fdwReason == DLL_PROCESS_ATTACH) {
        globalPlatformSetup();
    }
    return TRUE;
}
