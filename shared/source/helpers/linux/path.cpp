/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/path.h"

#include "shared/source/os_interface/linux/sys_calls.h"

#include <string>

namespace NEO {
bool pathExists(const std::string &path) {
    struct stat statbuf = {};

    if (NEO::SysCalls::stat(path.c_str(), &statbuf) == -1) {
        return false;
    }

    return (statbuf.st_mode & S_IFDIR) != 0;
}
} // namespace NEO