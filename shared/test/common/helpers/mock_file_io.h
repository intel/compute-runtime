/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"

bool virtualFileExists(const std::string &fileName);
void removeVirtualFile(const std::string &fileName);
std::unique_ptr<char[]> loadDataFromVirtualFile(
    const char *filename,
    size_t &retSize);