/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/file_io.h"

#include <map>
#include <sstream>
#include <string>

namespace NEO {
extern std::map<std::string, std::stringstream> virtualFileList;
extern std::map<std::string, std::stringstream> virtualFileListTestKernelsOnly;
} // namespace NEO

bool virtualFileExists(const std::string &fileName);
void removeVirtualFile(const std::string &fileName);
std::unique_ptr<char[]> loadDataFromVirtualFile(
    const char *filename,
    size_t &retSize);
std::unique_ptr<char[]> loadDataFromVirtualFileTestKernelsOnly(const char *filename, size_t &retSize);
