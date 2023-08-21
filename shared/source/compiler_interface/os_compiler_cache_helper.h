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
bool checkDefaultCacheDirSettings(std::string &cacheDir, SettingsReader *reader);
time_t getFileModificationTime(std::string &path);
size_t getFileSize(std::string &path);
} // namespace NEO