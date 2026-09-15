/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/source/helpers/product_config_helper.h"
#include "shared/source/utilities/io_functions.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "opencl/test/unit_test/offline_compiler/environment.h"
#include "opencl/test/unit_test/offline_compiler/mock/mock_offline_compiler.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <vector>

extern Environment *gEnvironment;

using namespace NEO;

using BmgOfflineCompilerTests = ::testing::Test;

TEST_F(BmgOfflineCompilerTests, givenBmgG21ReservedSteppingVersionValueWhenInitHardwareInfoForProductConfigThenInvalidDeviceIsReturned) {
    auto mockOfflineCompiler = std::make_unique<MockOfflineCompiler>();

    EXPECT_EQ(OCLOC_INVALID_DEVICE, mockOfflineCompiler->initHardwareInfoForProductConfig("20.1.1"));
    EXPECT_EQ(OCLOC_INVALID_DEVICE, mockOfflineCompiler->initHardwareInfoForProductConfig("20.1.4"));
}

TEST_F(BmgOfflineCompilerTests, givenBmgG21A0VersionValueWhenInitHardwareInfoForProductConfigThenSucceeds) {
    auto mockOfflineCompiler = std::make_unique<MockOfflineCompiler>();

    EXPECT_EQ(OCLOC_SUCCESS, mockOfflineCompiler->initHardwareInfoForProductConfig("20.1.0"));
}

TEST_F(BmgOfflineCompilerTests, givenBmgG21ReservedSteppingInDeviceOptionsWhenParsingCommandLineThenFails) {
    for (const std::string reservedStepping : {"20.1.1", "20.1.4"}) {
        auto mockOfflineCompiler = std::make_unique<MockOfflineCompiler>();

        std::vector<std::string> argv = {
            "ocloc",
            "-device_options", reservedStepping, "-cl-std=CL2.0",
            "-device", "bmg-g21"};

        StreamCapture capture;
        capture.captureStdout();
        int retVal = mockOfflineCompiler->parseCommandLine(argv.size(), argv);
        std::string output = capture.getCapturedStdout();

        EXPECT_EQ(OCLOC_INVALID_COMMAND_LINE, retVal);
        EXPECT_TRUE(output.find("Error: Invalid device in -device_options: " + reservedStepping) != std::string::npos) << output;
    }
}

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
