/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/source/os_interface/windows/sys_calls.h"

// Application detection is performed using the process name of the given application.

namespace NEO {
bool AILConfiguration::initProcessExecutableName() {
    const DWORD length = MAX_PATH;
    WCHAR processFilenameW[length];
    char processFilename[length] = "";
    auto status = SysCalls::getModuleFileName(nullptr, processFilenameW, MAX_PATH);

    if (status != 0) {
        std::wcstombs(processFilename, processFilenameW, MAX_PATH);
    }

    std::string_view pathView(processFilename);

    auto lastPosition = pathView.find_last_of("\\");

    pathView.remove_prefix(lastPosition + 1u);

    lastPosition = pathView.find(".exe");

    if (lastPosition != std::string_view::npos) {
        pathView.remove_suffix(pathView.size() - lastPosition);
    }

    processName = pathView;

    return status;
}
} // namespace NEO