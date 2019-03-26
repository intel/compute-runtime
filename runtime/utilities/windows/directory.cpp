/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/utilities/directory.h"

#include "runtime/os_interface/windows/windows_wrapper.h"

namespace NEO {

std::vector<std::string> Directory::getFiles(std::string &path) {
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
