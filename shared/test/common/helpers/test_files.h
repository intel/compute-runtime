/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <string>

extern std::string testFiles;
extern std::string testFilesApiSpecific;
extern std::string sharedFiles;
extern std::string sharedBuiltinsDir;
extern std::string clFiles;
extern std::string binaryNameSuffix;

void retrieveBinaryKernelFilename(std::string &outputFilename, const std::string &kernelName, const std::string &extension, const std::string &options = "");
void retrieveBinaryKernelFilenameApiSpecific(std::string &outputFilename, const std::string &kernelName, const std::string &extension, const std::string &options = "");
void appendBinaryNameSuffix(std::string &outputFileNameSuffix);