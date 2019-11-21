/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "Windows.h"

#include <string>

std::string getCurrentDirectoryOwn(std::string outDirForBuilds) {
    char buf[256];
    GetCurrentDirectoryA(256, buf);
    return std::string(buf) + "\\" + outDirForBuilds + "\\";
}
