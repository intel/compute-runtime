/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_files.h"

#include "shared/source/helpers/file_io.h"

#include "config.h"
#include "test_files_setup.h"

std::string testFiles("test_files/" NEO_ARCH "/");
std::string testFilesApiSpecific("test_files/" NEO_ARCH "/");
std::string clFiles("test_files/");
std::string sharedFiles(NEO_SHARED_TEST_FILES_DIR);
std::string sharedBuiltinsDir(NEO_SHARED_BUILTINS_DIR);
std::string binaryNameSuffix("");

void retrieveBinaryKernelFilename(std::string &outputFilename, const std::string &kernelName, const std::string &extension, const std::string &options) {
    if (outputFilename.length() > 0) {
        outputFilename.clear();
    }
    outputFilename.reserve(2 * testFiles.length());
    outputFilename.append(testFiles);
    outputFilename.append(kernelName);
    outputFilename.append(binaryNameSuffix);
    if (false == options.empty()) {
        outputFilename.append(options);
        outputFilename.append("_");
    }
    outputFilename.append(extension);
}

void retrieveBinaryKernelFilenameApiSpecific(std::string &outputFilename, const std::string &kernelName, const std::string &extension, const std::string &options) {
    if (outputFilename.length() > 0) {
        outputFilename.clear();
    }
    outputFilename.reserve(2 * testFilesApiSpecific.length());
    outputFilename.append(testFilesApiSpecific);
    outputFilename.append(kernelName);
    outputFilename.append(binaryNameSuffix);
    outputFilename.append(extension);
    outputFilename.append(options);
}

void appendBinaryNameSuffix(std::string &outputFileNameSuffix) {
    outputFileNameSuffix.append(binaryNameSuffix);
}
