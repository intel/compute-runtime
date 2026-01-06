/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/file_io.h"

bool virtualFileExists(const std::string &fileName);
void removeVirtualFile(const std::string &fileName);
std::unique_ptr<char[]> loadDataFromVirtualFile(
    const char *filename,
    size_t &retSize);
std::unique_ptr<char[]> loadDataFromVirtualFileTestKernelsOnly(const char *filename, size_t &retSize);
