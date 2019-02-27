/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/stdio.h"

#include <cstdint>
#include <string>

size_t loadDataFromFile(
    const char *filename,
    void *&pData);

void deleteDataReadFromFile(void *&pData);

size_t writeDataToFile(
    const char *filename,
    const void *pData,
    size_t dataSize);

bool fileExists(const std::string &fileName);
bool fileExistsHasSize(const std::string &fileName);
