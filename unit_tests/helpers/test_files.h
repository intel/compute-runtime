/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <string>

extern std::string testFiles;
extern std::string clFiles;
extern std::string binaryNameSuffix;

void retrieveBinaryKernelFilename(std::string &outputFilename, const std::string &kernelName, const std::string &extension, const std::string &options = "");
