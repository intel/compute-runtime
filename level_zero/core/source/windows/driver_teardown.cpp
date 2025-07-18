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

BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) { // NOLINT(readability-identifier-naming)
    if (fdwReason == DLL_PROCESS_ATTACH) {
        L0::globalDriverSetup();
    }
    if (fdwReason == DLL_PROCESS_DETACH) {
        /* If lpvReserved is non-NULL with DLL_PROCESS_DETACH, the process is terminating,
         * clean up should be skipped according to the DllMain spec. */
        bool processTermination = lpvReserved != nullptr;
        L0::globalDriverTeardown(processTermination);

        if (processTermination) {
            return TRUE;
        }

        if (L0::globalOsSysmanDriver != nullptr) {
            delete L0::globalOsSysmanDriver;
            L0::globalOsSysmanDriver = nullptr;
        }
    }
    return TRUE;
}
