/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <sstream>
#include <string>
#include <sys/types.h>
#include <unistd.h>

std::string getPath() {
    char exepath[128] = {0};
    std::stringstream ss;
    ss << "/proc/" << getpid() << "/exe";
    if (readlink(ss.str().c_str(), exepath, 128) != -1) {
        std::string path = std::string(exepath);
        path = path.substr(0, path.find_last_of('/') + 1);
        return path;
    } else {
        return std::string("");
    }
}
