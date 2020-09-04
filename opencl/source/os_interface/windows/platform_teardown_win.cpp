/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/platform/platform.h"

using namespace NEO;

BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_DETACH) {
        delete platformsImpl;
    }
    if (fdwReason == DLL_PROCESS_ATTACH) {
        platformsImpl = new std::vector<std::unique_ptr<Platform>>;
    }
    return TRUE;
}
