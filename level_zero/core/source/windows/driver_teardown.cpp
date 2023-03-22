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

using namespace L0;

BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_DETACH) {
        std::string loaderLibraryName = L0::loaderLibraryFilename + ".dll";
        loaderDriverTeardown(loaderLibraryName);
        globalDriverTeardown();
        if (GlobalOsSysmanDriver != nullptr) {
            delete GlobalOsSysmanDriver;
            GlobalOsSysmanDriver = nullptr;
        }
    }
    return TRUE;
}
