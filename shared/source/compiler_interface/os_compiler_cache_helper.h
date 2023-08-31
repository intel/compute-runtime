/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <string>

namespace NEO {
class SettingsReader;
int64_t defaultCacheEnabled();
std::string makePath(const std::string &lhs, const std::string &rhs);
bool checkDefaultCacheDirSettings(std::string &cacheDir, SettingsReader *reader);
time_t getFileModificationTime(const std::string &path);
size_t getFileSize(const std::string &path);
} // namespace NEO