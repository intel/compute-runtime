/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/offline_compiler/source/offline_compiler.h"

#include "environment.h"
#include "gtest/gtest.h"

#include <string>

extern Environment *gEnvironment;

using namespace std::string_literals;

TEST(OclocApiTests, WhenGoodArgsAreGivenThenSuccessIsReturned) {
    const char *argv[] = {
        "ocloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(retVal, NEO::SUCCESS);
    EXPECT_EQ(std::string::npos, output.find("Command was: ocloc -file test_files/copybuffer.cl -device "s + argv[4]));
}

TEST(OclocApiTests, WhenArgsWithMissingFileAreGivenThenErrorMessageIsProduced) {
    const char *argv[] = {
        "ocloc",
        "-file",
        "test_files/IDoNotExist.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    testing::internal::CaptureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(retVal, NEO::INVALID_FILE);
    EXPECT_NE(std::string::npos, output.find("Command was: ocloc -file test_files/IDoNotExist.cl -device "s + argv[4]));
}
