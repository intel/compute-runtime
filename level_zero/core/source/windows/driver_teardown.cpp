/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/sysman/source/sysman_driver_handle_imp.h"

#include <windows.h>

using namespace L0;

BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_DETACH) {
        if (GlobalDriver != nullptr) {
            delete GlobalDriver;
            GlobalDriver = nullptr;
        }
        if (Sysman::GlobalSysmanDriver != nullptr) {
            delete Sysman::GlobalSysmanDriver;
            Sysman::GlobalSysmanDriver = nullptr;
        }
    }
    return TRUE;
}
