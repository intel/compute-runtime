/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/utilities/directory.h"
#include "runtime/os_interface/windows/windows_wrapper.h"

namespace OCLRT {

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
}; // namespace OCLRT
