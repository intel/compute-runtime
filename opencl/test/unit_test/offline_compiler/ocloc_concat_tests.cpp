/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/source/device_binary_format/ar/ar_encoder.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"

#include "gtest/gtest.h"
#include "mock/mock_argument_helper.h"
#include "mock/mock_ocloc_concat.h"
#include "neo_aot_platforms.h"

#include <array>

namespace NEO {
TEST(OclocConcatTest, GivenNoArgumentsWhenInitializingThenErrorIsReturned) {
    MockOclocArgHelper::FilesMap mockArgHelperFilesMap{};
    MockOclocArgHelper mockArgHelper{mockArgHelperFilesMap};
    auto oclocConcat = MockOclocConcat(&mockArgHelper);
    std::vector<std::string> args = {"ocloc", "concat"};

    mockArgHelper.messagePrinter.setSuppressMessages(true);
    auto error = oclocConcat.initialize(args);
    const auto output = mockArgHelper.getLog();

    EXPECT_EQ(static_cast<uint32_t>(OCLOC_INVALID_COMMAND_LINE), error);
    const std::string expectedOutput = "No files to concatenate were provided.\n";
    EXPECT_EQ(expectedOutput, output);
}

TEST(OclocConcatTest, GivenMissingFilesWhenInitializingThenErrorIsReturned) {
    MockOclocArgHelper::FilesMap mockArgHelperFilesMap{};
    MockOclocArgHelper mockArgHelper{mockArgHelperFilesMap};
    auto oclocConcat = MockOclocConcat(&mockArgHelper);
    std::vector<std::string> args = {"ocloc", "concat", "fatBinary1.ar", "fatBinary2.ar"};

    mockArgHelper.messagePrinter.setSuppressMessages(true);
    auto error = oclocConcat.initialize(args);
    const auto output = mockArgHelper.getLog();

    EXPECT_EQ(static_cast<uint32_t>(OCLOC_INVALID_COMMAND_LINE), error);
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

    mockArgHelper.messagePrinter.setSuppressMessages(true);
    auto error = oclocConcat.initialize(args);
    const auto output = mockArgHelper.getLog();

    EXPECT_EQ(static_cast<uint32_t>(OCLOC_SUCCESS), error);
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

    mockArgHelper.messagePrinter.setSuppressMessages(true);
    auto error = oclocConcat.initialize(args);
    const auto output = mockArgHelper.getLog();

    EXPECT_EQ(static_cast<uint32_t>(OCLOC_INVALID_COMMAND_LINE), error);
    const std::string expectedOutput = "Missing out file name after \"-out\" argument\n";
    EXPECT_EQ(expectedOutput, output);
}

TEST(OclocConcatTest, GivenErrorDuringDecodingArWhenConcatenatingThenErrorIsReturned) {
    MockOclocArgHelper::FilesMap mockArgHelperFilesMap{
        {"fatBinary1.ar", "!<arch>\nInvalidAr"},
        {"fatBinary2.ar", "!<arch>\nfatBinary2Data"}};
    MockOclocArgHelper mockArgHelper{mockArgHelperFilesMap};
    auto oclocConcat = MockOclocConcat(&mockArgHelper);
    mockArgHelper.messagePrinter.setSuppressMessages(true);
    oclocConcat.shouldFailDecodingAr = true;
    oclocConcat.fileNamesToConcat = {"fatBinary1.ar",
                                     "fatBinary2.ar"};
    auto error = oclocConcat.concatenate();
    const auto output = mockArgHelper.messagePrinter.getLog().str();
    EXPECT_EQ(static_cast<uint32_t>(OCLOC_INVALID_FILE), error);
    EXPECT_EQ("fatBinary1.ar : Error while decoding AR file\n", output);
}

TEST(OclocConcatTest, GivenBinaryFileNonZebinWhenConcatenatingThenErrorIsReturned) {
    MockOclocArgHelper::FilesMap mockArgHelperFilesMap{
        {"binary.bin", "NOT Zebin"}};
    MockOclocArgHelper mockArgHelper{mockArgHelperFilesMap};
    auto oclocConcat = MockOclocConcat(&mockArgHelper);
    mockArgHelper.messagePrinter.setSuppressMessages(true);
    oclocConcat.fileNamesToConcat = {"binary.bin"};
    auto error = oclocConcat.concatenate();
    const auto output = mockArgHelper.messagePrinter.getLog().str();
    EXPECT_EQ(static_cast<uint32_t>(OCLOC_INVALID_FILE), error);
    EXPECT_EQ("binary.bin : Not a zebin file\n", output);
}

TEST(OclocConcatTest, GivenZebinWithoutAOTProductConfigWhenConcatenatingThenErrorIsReturned) {
    ZebinTestData::ValidEmptyProgram zebin64;
    ZebinTestData::ValidEmptyProgram<Elf::EI_CLASS_32> zebin32;
    for (auto zebin : {&zebin64.storage, &zebin32.storage}) {
        MockOclocArgHelper::FilesMap mockArgHelperFilesMap{
            {"zebin.bin", std::string(reinterpret_cast<const char *>(zebin->data()), zebin->size())}};
        MockOclocArgHelper mockArgHelper{mockArgHelperFilesMap};
        auto oclocConcat = MockOclocConcat(&mockArgHelper);
        mockArgHelper.messagePrinter.setSuppressMessages(true);
        oclocConcat.fileNamesToConcat = {"zebin.bin"};
        auto error = oclocConcat.concatenate();
        const auto output = mockArgHelper.messagePrinter.getLog().str();
        EXPECT_EQ(static_cast<uint32_t>(OCLOC_INVALID_FILE), error);
        EXPECT_EQ("zebin.bin : Couldn't find AOT product configuration in intelGTNotes section.\n", output);
    }
}

TEST(OclocConcatTest, GivenZebinWithAOTNoteAndFatBinaryWhenConcatenatingThenCorrectFatBinaryIsProduced) {
    std::vector<uint8_t> fatBinary;
    {
        std::array<uint8_t, 32> file{0};
        NEO::Ar::ArEncoder arEncoder(true);
        arEncoder.appendFileEntry("10.0.0", ArrayRef<const uint8_t>::fromAny(file.data(), file.size()));
        arEncoder.appendFileEntry("11.0.0", ArrayRef<const uint8_t>::fromAny(file.data(), file.size()));
        fatBinary = arEncoder.encode();
    }

    ZebinTestData::ValidEmptyProgram zebin;
    {
        AOT::PRODUCT_CONFIG productConfig = AOT::PRODUCT_CONFIG::TGL; // 12.0.0
        zebin.appendSection(Elf::SHT_NOTE, Zebin::Elf::SectionNames::noteIntelGT,
                            ZebinTestData::createIntelGTNoteSection(versionToString(NEO::Zebin::ZeInfo::zeInfoDecoderVersion), productConfig));
    }

    MockOclocArgHelper::FilesMap mockArgHelperFilesMap{
        {"binary.bin", std::string(reinterpret_cast<const char *>(zebin.storage.data()), zebin.storage.size())},
        {"fatBinary.ar", std::string(reinterpret_cast<const char *>(fatBinary.data()), fatBinary.size())}};
    MockOclocArgHelper mockArgHelper{mockArgHelperFilesMap};
    mockArgHelper.interceptOutput = true;
    mockArgHelper.messagePrinter.setSuppressMessages(true);

    auto oclocConcat = MockOclocConcat(&mockArgHelper);
    oclocConcat.fileNamesToConcat = {
        "fatBinary.ar",
        "binary.bin",
    };

    auto error = oclocConcat.concatenate();
    const auto output = mockArgHelper.messagePrinter.getLog().str();

    EXPECT_EQ(static_cast<uint32_t>(OCLOC_SUCCESS), error);
    EXPECT_TRUE(output.empty());
    EXPECT_EQ(1U, mockArgHelper.interceptedFiles.size());

    std::string errors, warnings;
    auto &concatedFatBinary = mockArgHelper.interceptedFiles["concat.ar"];
    auto concatedAr = NEO::Ar::decodeAr(ArrayRef<const uint8_t>::fromAny(reinterpret_cast<const uint8_t *>(concatedFatBinary.data()), concatedFatBinary.size()), errors, warnings);
    EXPECT_TRUE(errors.empty());
    EXPECT_TRUE(warnings.empty());
    ASSERT_EQ(6U, concatedAr.files.size());
    EXPECT_EQ("10.0.0", concatedAr.files[1].fileName);
    EXPECT_EQ("11.0.0", concatedAr.files[3].fileName);
    EXPECT_EQ("12.0.0", concatedAr.files[5].fileName);
}

} // namespace NEO
