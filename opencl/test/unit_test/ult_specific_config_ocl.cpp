/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/tests_configuration.h"

#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/ult_config_listener.h"

#include "test_files_setup.h"
using namespace NEO;

void applyWorkarounds() {
    platformsImpl = new std::vector<std::unique_ptr<Platform>>;
    platformsImpl->reserve(1);
}

void cleanTestHelpers() {
    delete platformsImpl;
}

bool isPlatformSupported(const HardwareInfo &hwInfoForTests) {
    return true;
}

void setupTestFiles(std::string testBinaryFiles, int32_t revId) {
    testBinaryFiles.append("/");
    testBinaryFiles.append(binaryNameSuffix);
    testBinaryFiles.append("/");
    testBinaryFiles.append(std::to_string(revId));
    testBinaryFiles.append("/");
    testBinaryFiles.append(testFiles);
    testFiles = testBinaryFiles;

    std::string nClFiles = NEO_OPENCL_TEST_FILES_DIR;
    nClFiles.append("/");
    clFiles = nClFiles;
}

std::string getBaseExecutionDir() {
    if (testMode != TestMode::aubTests) {
        return "opencl/";
    }
    return "";
}

void addUltListener(::testing::TestEventListeners &listeners) {
    listeners.Append(new UltConfigListener);
}
