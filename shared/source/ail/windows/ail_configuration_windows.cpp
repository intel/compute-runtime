/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/source/os_interface/windows/sys_calls.h"

namespace NEO {
bool AILConfiguration::initProcessExecutableName() {
    const DWORD length = MAX_PATH;
    WCHAR processFilenameW[length];
    char processFilename[length] = "";
    auto status = SysCalls::getModuleFileName(nullptr, processFilenameW, MAX_PATH);

    if (status != 0) {
        std::wcstombs(processFilename, processFilenameW, MAX_PATH);
    }

    return status;
}
} // namespace NEO