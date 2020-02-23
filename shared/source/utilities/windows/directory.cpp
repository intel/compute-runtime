/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/directory.h"

#include "shared/source/os_interface/windows/windows_wrapper.h"

namespace NEO {

std::vector<std::string> Directory::getFiles(const std::string &path) {
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
}; // namespace NEO
