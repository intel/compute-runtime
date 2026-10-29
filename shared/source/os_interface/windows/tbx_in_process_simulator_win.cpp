/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/tbx_in_process_simulator.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"

namespace NEO {

const char *const TbxInProcessSimulator::mainFunctionName = "?fs_main@@YAHHPEAPEBD@Z";

int *TbxInProcessSimulator::getLibraryLoadFlags() {
    static int libraryLoadFlags = LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;
    return &libraryLoadFlags;
}

} // namespace NEO
