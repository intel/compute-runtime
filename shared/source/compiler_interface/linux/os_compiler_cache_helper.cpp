/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/os_compiler_cache_helper.h"

#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/utilities/debug_settings_reader.h"

namespace NEO {
bool createCompilerCachePath(std::string &cacheDir) {
    if (NEO::SysCalls::pathExists(cacheDir)) {
        if (NEO::SysCalls::pathExists(cacheDir + "neo_compiler_cache")) {
            cacheDir = cacheDir + "neo_compiler_cache";
            return true;
        }

        if (NEO::SysCalls::mkdir(cacheDir + "neo_compiler_cache") == 0) {
            cacheDir = cacheDir + "neo_compiler_cache";
            return true;
        } else {
            if (errno == EEXIST) {
                cacheDir = cacheDir + "neo_compiler_cache";
                return true;
            }
        }
    }

    cacheDir = "";
    return false;
}

bool checkDefaultCacheDirSettings(std::string &cacheDir, SettingsReader *reader) {
    std::string emptyString = "";
    cacheDir = reader->getSetting(reader->appSpecificLocation("XDG_CACHE_HOME"), emptyString);

    if (cacheDir.empty()) {
        cacheDir = reader->getSetting(reader->appSpecificLocation("HOME"), emptyString);
        if (cacheDir.empty()) {
            return false;
        }

        cacheDir = cacheDir + ".cache/";

        return createCompilerCachePath(cacheDir);
    }

    if (NEO::SysCalls::pathExists(cacheDir)) {
        return createCompilerCachePath(cacheDir);
    }

    return false;
}
} // namespace NEO