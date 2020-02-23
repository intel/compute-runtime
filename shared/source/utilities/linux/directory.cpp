/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/directory.h"

#include <cstdio>
#include <dirent.h>

namespace NEO {

std::vector<std::string> Directory::getFiles(const std::string &path) {
    std::vector<std::string> files;

    DIR *dir = opendir(path.c_str());
    if (dir == nullptr) {
        return files;
    }

    struct dirent *entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        std::string fullPath;
        fullPath += path;
        fullPath += "/";
        fullPath += entry->d_name;

        files.push_back(fullPath);
    }

    closedir(dir);
    return files;
}
}; // namespace NEO
