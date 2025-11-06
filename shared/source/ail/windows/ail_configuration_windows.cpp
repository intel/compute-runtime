/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/windows/sys_calls.h"

// Application detection is performed using the process name of the given application.

namespace NEO {
bool AILConfiguration::initProcessExecutableName() {
    const DWORD length = MAX_PATH;
    WCHAR processFilenameW[length];
    char processFilename[length] = "";
    auto status = SysCalls::getModuleFileName(nullptr, processFilenameW, MAX_PATH);

    if (status != 0) {
        std::wstring_view pathView(processFilenameW);

        auto lastPosition = pathView.find_last_of(L"\\");
        UNRECOVERABLE_IF(lastPosition == std::wstring_view::npos);

        pathView.remove_prefix(lastPosition + 1u);

        lastPosition = pathView.find(L".exe");

        if (lastPosition != std::wstring_view::npos) {
            pathView.remove_suffix(pathView.size() - lastPosition);
        }

        std::wcstombs(processFilename, pathView.data(), pathView.size());
        processName = processFilename;
    }

    return status;
}
} // namespace NEO
