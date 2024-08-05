/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/directory.h"

#include "shared/source/helpers/debug_helpers.h"

#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

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

void Directory::createDirectory(const std::string &path) {
    const mode_t mode = 0777; // 777 in base 8
    [[maybe_unused]] auto status = mkdir(path.c_str(), mode);
    DEBUG_BREAK_IF(status != 0 && errno != EEXIST);
}

} // namespace NEO