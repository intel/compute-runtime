/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/path.h"

#include "shared/source/os_interface/windows/sys_calls.h"

#include <string>

namespace NEO {
bool pathExists(const std::string &path) {
    DWORD ret = NEO::SysCalls::getFileAttributesA(path.c_str());

    if (ret == INVALID_FILE_ATTRIBUTES) {
        return false;
    }

    return (ret & FILE_ATTRIBUTE_DIRECTORY) != 0;
}
} // namespace NEO