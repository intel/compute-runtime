/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/tests_configuration.h"

#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/ult_config_listener.h"

#include "test_files_setup.h"

namespace NEO {
const char *apiName = "OCL";
}
using namespace NEO;

void applyWorkarounds() {
    platformsImpl = new std::vector<std::unique_ptr<Platform>>;
    platformsImpl->reserve(8);
}

void cleanTestHelpers() {
    delete platformsImpl;
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
    if (!isAubTestMode(testMode)) {
        return "opencl/";
    }
    return "";
}

bool isChangeDirectoryRequired() {
    return testMode == TestMode::aubTests;
}

void addUltListener(::testing::TestEventListeners &listeners) {
    listeners.Append(new UltConfigListener);
}
