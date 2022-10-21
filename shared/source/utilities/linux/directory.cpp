/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/directory.h"

#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace NEO::Directory {

std::vector<std::string> getFiles(const std::string &path) {
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

void createDirectory(const std::string &path) {
    const mode_t mode = 0777; // 777 in base 8
    mkdir(path.c_str(), mode);
}
}; // namespace NEO::Directory
