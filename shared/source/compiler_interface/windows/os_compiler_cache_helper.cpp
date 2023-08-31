/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/os_compiler_cache_helper.h"

namespace NEO {
int64_t defaultCacheEnabled() {
    return 0l;
}
std::string makePath(const std::string &lhs, const std::string &rhs) {
    return lhs + rhs;
}
bool checkDefaultCacheDirSettings(std::string &cacheDir, SettingsReader *reader) {
    return false;
}
time_t getFileModificationTime(const std::string &path) {
    return 0;
}
size_t getFileSize(const std::string &path) {
    return 0;
}
} // namespace NEO