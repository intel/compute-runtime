/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/windows_wrapper.h"

#include "opencl/source/global_teardown/global_platform_teardown.h"

using namespace NEO;

BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) { // NOLINT(readability-identifier-naming)
    if (fdwReason == DLL_PROCESS_DETACH) {
        globalPlatformTeardown();
    }
    if (fdwReason == DLL_PROCESS_ATTACH) {
        globalPlatformSetup();
    }
    return TRUE;
}
