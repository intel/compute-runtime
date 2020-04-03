/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_files.h"

#include "shared/source/helpers/file_io.h"

#include "config.h"

std::string testFiles("test_files/" NEO_ARCH "/");
std::string clFiles("test_files/");
std::string binaryNameSuffix("");

void retrieveBinaryKernelFilename(std::string &outputFilename, const std::string &kernelName, const std::string &extension, const std::string &options) {
    if (outputFilename.length() > 0) {
        outputFilename.clear();
    }
    outputFilename.reserve(2 * testFiles.length());
    outputFilename.append(testFiles);
    outputFilename.append(kernelName);
    outputFilename.append(binaryNameSuffix);
    outputFilename.append(extension);
    outputFilename.append(options);

    if (!fileExists(outputFilename) && (extension == ".bc")) {
        retrieveBinaryKernelFilename(outputFilename, kernelName, ".spv", options);
    }
}
