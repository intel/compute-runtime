/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/directory.h"

#include "shared/source/os_interface/windows/windows_wrapper.h"

#include <direct.h>

namespace NEO::Directory {

std::vector<std::string> getFiles(const std::string &path) {
    std::vector<std::string> files;
    std::string newPath;

    WIN32_FIND_DATAA ffd;
    HANDLE hFind = INVALID_HANDLE_VALUE;

    if (path.c_str()[path.size() - 1] == '/') {
        return files;
    } else {
        newPath = path + "/*";
    }

    hFind = FindFirstFileA(newPath.c_str(), &ffd);

    if (INVALID_HANDLE_VALUE == hFind) {
        return files;
    }

    do {
        files.push_back(path + "/" + ffd.cFileName);
    } while (FindNextFileA(hFind, &ffd) != 0);

    FindClose(hFind);
    return files;
}

void createDirectory(const std::string &path) {
    _mkdir(path.c_str());
}
}; // namespace NEO::Directory
