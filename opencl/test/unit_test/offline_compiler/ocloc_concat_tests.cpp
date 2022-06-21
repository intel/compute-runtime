/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_error_code.h"

#include "gtest/gtest.h"
#include "mock/mock_argument_helper.h"
#include "mock/mock_ocloc_concat.h"

namespace NEO {
TEST(OclocConcatTest, GivenNoArgumentsWhenInitializingThenErrorIsReturned) {
    MockOclocArgHelper::FilesMap mockArgHelperFilesMap{};
    MockOclocArgHelper mockArgHelper{mockArgHelperFilesMap};
    auto oclocConcat = MockOclocConcat(&mockArgHelper);
    std::vector<std::string> args = {"ocloc", "concat"};

    ::testing::internal::CaptureStdout();
    auto error = oclocConcat.initialize(args);
    const auto output = ::testing::internal::GetCapturedStdout();

    EXPECT_EQ(static_cast<uint32_t>(OclocErrorCode::INVALID_COMMAND_LINE), error);
    const std::string expectedOutput = "No files to concatenate were provided.\n";
    EXPECT_EQ(expectedOutput, output);
}

TEST(OclocConcatTest, GivenMissingFilesWhenInitializingThenErrorIsReturned) {
    MockOclocArgHelper::FilesMap mockArgHelperFilesMap{};
    MockOclocArgHelper mockArgHelper{mockArgHelperFilesMap};
    auto oclocConcat = MockOclocConcat(&mockArgHelper);
    std::vector<std::string> args = {"ocloc", "concat", "fatBinary1.ar", "fatBinary2.ar"};

    ::testing::internal::CaptureStdout();
    auto error = oclocConcat.initialize(args);
    const auto output = ::testing::internal::GetCapturedStdout();

    EXPECT_EQ(static_cast<uint32_t>(OclocErrorCode::INVALID_COMMAND_LINE), error);
    const std::string expectedOutput = "fatBinary1.ar doesn't exist!\nfatBinary2.ar doesn't exist!\n";
    EXPECT_EQ(expectedOutput, output);
}

TEST(OclocConcatTest, GivenValidArgsWhenInitializingThenFileNamesToConcatAndOutputFileNameAreSetCorrectlyAndSuccessIsReturned) {
    MockOclocArgHelper::FilesMap mockArgHelperFilesMap{
        {"fatBinary1.ar", "fatBinary1Data"},
        {"fatBinary2.ar", "fatBinary2Data"}};
    MockOclocArgHelper mockArgHelper{mockArgHelperFilesMap};
    auto oclocConcat = MockOclocConcat(&mockArgHelper);
    std::vector<std::string> args = {"ocloc", "concat", "fatBinary1.ar", "fatBinary2.ar", "-out", "fatBinary.ar"};

    ::testing::internal::CaptureStdout();
    auto error = oclocConcat.initialize(args);
    const auto output = ::testing::internal::GetCapturedStdout();

    EXPECT_EQ(static_cast<uint32_t>(OclocErrorCode::SUCCESS), error);
    EXPECT_TRUE(output.empty());

    EXPECT_EQ(args[2], oclocConcat.fileNamesToConcat[0]);
    EXPECT_EQ(args[3], oclocConcat.fileNamesToConcat[1]);
    EXPECT_EQ(args[5], oclocConcat.fatBinaryName);
}

TEST(OclocConcatTest, GivenMissingOutFileNameAfterOutArgumentWhenInitalizingThenErrorIsReturned) {
    MockOclocArgHelper::FilesMap mockArgHelperFilesMap{};
    MockOclocArgHelper mockArgHelper{mockArgHelperFilesMap};
    auto oclocConcat = MockOclocConcat(&mockArgHelper);
    std::vector<std::string> args = {"ocloc", "concat", "fatBinary1.ar", "fatBinary2.ar", "-out"};

    ::testing::internal::CaptureStdout();
    auto error = oclocConcat.initialize(args);
    const auto output = ::testing::internal::GetCapturedStdout();

    EXPECT_EQ(static_cast<uint32_t>(OclocErrorCode::INVALID_COMMAND_LINE), error);
    const std::string expectedOutput = "Missing out file name after \"-out\" argument\n";
    EXPECT_EQ(expectedOutput, output);
}

TEST(OclocConcatTest, GivenErrorDuringDecodingArWhenConcatenatingThenErrorIsReturned) {
    MockOclocArgHelper::FilesMap mockArgHelperFilesMap{
        {"fatBinary1.ar", "fatBinary1Data"},
        {"fatBinary2.ar", "fatBinary2Data"}};
    MockOclocArgHelper mockArgHelper{mockArgHelperFilesMap};
    auto oclocConcat = MockOclocConcat(&mockArgHelper);
    oclocConcat.shouldFailDecodingAr = true;
    oclocConcat.fileNamesToConcat = {"fatBinary1.ar",
                                     "fatBinary2.ar"};

    ::testing::internal::CaptureStdout();
    auto error = oclocConcat.concatenate();
    const auto output = ::testing::internal::GetCapturedStdout();

    EXPECT_EQ(static_cast<uint32_t>(OclocErrorCode::INVALID_FILE), error);
    EXPECT_EQ(MockOclocConcat::decodeArErrorMessage.str(), output);
}

} // namespace NEO