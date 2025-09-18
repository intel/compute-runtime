/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include <string_view>

// Application detection is performed using the process name of the given application.

namespace NEO {
bool AILConfiguration::initProcessExecutableName() {
    char path[512] = {0};
    int result = SysCalls::readlink("/proc/self/exe", path, sizeof(path) - 1);

    if (result == -1) {
        return false;
    }

    path[result] = '\0';
    std::string_view pathView(path);

    auto lastPosition = pathView.find_last_of("/");
    UNRECOVERABLE_IF(lastPosition == std::string_view::npos);

    pathView.remove_prefix(lastPosition + 1u);

    processName = pathView;

    return true;
}

} // namespace NEO