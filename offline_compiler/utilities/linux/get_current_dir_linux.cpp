/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <string>
#include <unistd.h>

std::string getCurrentDirectoryOwn(std::string outDirForBuilds) {
    char buf[256];
    if (getcwd(buf, sizeof(buf)) != NULL)
        return std::string(buf) + "/" + outDirForBuilds + "/";
    else
        return std::string("./") + outDirForBuilds + "/";
}
