/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/os_compiler_cache_helper.h"

namespace NEO {
bool checkDefaultCacheDirSettings(std::string &cacheDir, SettingsReader *reader) {
    return false;
}
time_t getFileModificationTime(std::string &path) {
    return 0;
}
size_t getFileSize(std::string &path) {
    return 0;
}
} // namespace NEO