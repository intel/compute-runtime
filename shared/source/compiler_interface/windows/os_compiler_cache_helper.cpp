/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/os_compiler_cache_helper.h"

#include "shared/source/helpers/path.h"
#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/source/utilities/debug_settings_reader.h"

#include <ShlObj.h>
#include <algorithm>

namespace NEO {
std::string getKnownFolderPath(REFKNOWNFOLDERID rfid) {
    PWSTR path = nullptr;
    auto result = SysCalls::shGetKnownFolderPath(rfid, 0, nullptr, &path);
    if (result != S_OK) {
        SysCalls::coTaskMemFree(path);
        return std::string();
    }
    std::wstring temp(path);
    SysCalls::coTaskMemFree(path);
    std::string ret(temp.length(), 0);
    std::transform(temp.begin(), temp.end(), ret.begin(), [](wchar_t c) {
        return static_cast<char>(c);
    });
    return ret;
}

bool createCompilerCachePath(std::string &cacheDir) {
    if (pathExists(cacheDir)) {
        cacheDir = joinPath(cacheDir, "neo_compiler_cache");
        if (pathExists(cacheDir)) {
            return true;
        }

        auto result = SysCalls::createDirectoryA(cacheDir.c_str(), NULL);
        if (result) {
            return true;
        }

        if (SysCalls::getLastError() == ERROR_ALREADY_EXISTS) {
            return true;
        }
    }

    cacheDir = "";
    return false;
}

bool checkDefaultCacheDirSettings(std::string &cacheDir, NEO::EnvironmentVariableReader &reader) {
    cacheDir = getKnownFolderPath(FOLDERID_LocalAppData);

    if (cacheDir.empty()) {
        return false;
    }

    cacheDir = joinPath(cacheDir, "NEO\\");
    if (!pathExists(cacheDir)) {
        SysCalls::createDirectoryA(cacheDir.c_str(), NULL);
    }

    if (pathExists(cacheDir)) {
        return createCompilerCachePath(cacheDir);
    }

    cacheDir = "";
    return false;
}

time_t getFileModificationTime(const std::string &path) {
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    hFind = SysCalls::findFirstFileA(path.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        return 0;
    }
    ULARGE_INTEGER uli;
    uli.LowPart = ffd.ftLastWriteTime.dwLowDateTime;
    uli.HighPart = ffd.ftLastWriteTime.dwHighDateTime;
    SysCalls::findClose(hFind);
    return uli.QuadPart;
}

size_t getFileSize(const std::string &path) {
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    hFind = SysCalls::findFirstFileA(path.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        return 0u;
    }
    SysCalls::findClose(hFind);
    return static_cast<size_t>((ffd.nFileSizeHigh * (MAXDWORD + 1)) + ffd.nFileSizeLow);
}

} // namespace NEO