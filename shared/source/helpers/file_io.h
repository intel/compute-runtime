/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>

std::unique_ptr<char[]> loadDataFromFile(
    const char *filename,
    size_t &retSize);

size_t writeDataToFile(
    const char *filename,
    const void *pData,
    size_t dataSize);

bool fileExists(const std::string &fileName);
bool fileExistsHasSize(const std::string &fileName);
void dumpFileIncrement(const char *data, size_t dataSize, const std::string &filename, const std::string &extension);
