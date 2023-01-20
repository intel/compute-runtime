/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/windows_wrapper.h"

#include <string>

std::string getCurrentDirectoryOwn(std::string outDirForBuilds) {
    char buf[256];
    GetCurrentDirectoryA(256, buf);
    return std::string(buf) + "\\" + outDirForBuilds + "\\";
}
