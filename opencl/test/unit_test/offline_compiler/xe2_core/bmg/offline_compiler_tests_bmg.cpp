/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/source/utilities/io_functions.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "opencl/test/unit_test/offline_compiler/environment.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <vector>

extern Environment *gEnvironment;

using namespace NEO;

using BmgOfflineCompilerTests = ::testing::Test;

TEST_F(BmgOfflineCompilerTests, givenBmgWhenCompilingWithUseLscIntrinsicsFlagThenFlagIsProperlyPassed) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {
        std::filesystem::path filePath = filename;
        std::string fileNameWithExtension = filePath.filename().string();

        std::vector<std::string> expectedFiles = {
            "some_kernel.cl"};

        auto itr = std::find(expectedFiles.begin(), expectedFiles.end(), std::string(fileNameWithExtension));
        if (itr != expectedFiles.end()) {
            return reinterpret_cast<FILE *>(0x40);
        }
        return NULL;
    });
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFclose(&NEO::IoFunctions::fclosePtr, [](FILE *stream) -> int {
        return 0;
    });

    const char *argv[] = {
        "ocloc",
        "-q",
        "-file",
        "test_files/NonExistentFile.cl",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-options",
        "-DUSE_LSC_INTRINSICS_WB",
        "-v"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = capture.getCapturedStdout();

    EXPECT_NE(retVal, OCLOC_SUCCESS);
    EXPECT_NE(std::string::npos, output.find("-DUSE_LSC_INTRINSICS_WB")) << output;
}
