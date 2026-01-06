/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_validator.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_argument_helper.h"

#include "gtest/gtest.h"

TEST(OclocValidate, WhenFileArgIsMissingThenFail) {
    std::map<std::string, std::string> files;
    MockOclocArgHelper argHelper{files};
    argHelper.getPrinterRef().setSuppressMessages(true);
    int res = Ocloc::validate({}, &argHelper);
    EXPECT_EQ(-1, res);
    std::string oclocStdout = argHelper.getPrinterRef().getLog().str();
    EXPECT_STREQ("Error : Mandatory argument -file is missing.\n", oclocStdout.c_str());
}

TEST(OclocValidate, WhenInputFileIsMissingThenFail) {
    MockOclocArgHelper::FilesMap files;
    MockOclocArgHelper argHelper{files};
    argHelper.getPrinterRef().setSuppressMessages(true);
    int res = Ocloc::validate({"-file", "src.gen"}, &argHelper);
    EXPECT_EQ(-1, res);
    std::string oclocStdout = argHelper.getPrinterRef().getLog().str();
    EXPECT_STREQ("Error : Input file missing : src.gen\n", oclocStdout.c_str());
}

TEST(OclocValidate, WhenInputFileIsAvailableThenLogItsSize) {
    MockOclocArgHelper::FilesMap files{{"src.gen", "01234567"}};
    MockOclocArgHelper argHelper{files};
    argHelper.getPrinterRef().setSuppressMessages(true);
    int res = Ocloc::validate({"-file", "src.gen"}, &argHelper);
    EXPECT_NE(0, res);
    std::string oclocStdout = argHelper.getPrinterRef().getLog().str();
    EXPECT_NE(nullptr, strstr(oclocStdout.c_str(), "Validating : src.gen (8 bytes).\n")) << oclocStdout;
}

TEST(OclocValidate, WhenInputFileIsNotZebinThenFail) {
    MockOclocArgHelper::FilesMap files{{"src.gen", "01234567"}};
    MockOclocArgHelper argHelper{files};
    argHelper.getPrinterRef().setSuppressMessages(true);
    int res = Ocloc::validate({"-file", "src.gen"}, &argHelper);
    EXPECT_EQ(-2, res);
    std::string oclocStdout = argHelper.getPrinterRef().getLog().str();
    EXPECT_NE(nullptr, strstr(oclocStdout.c_str(), "Input is not a Zebin file (not elf or wrong elf object file type)")) << oclocStdout;
}

TEST(OclocValidate, WhenInputIsValidZebinThenReturnSucceed) {
    ZebinTestData::ValidEmptyProgram zebin;
    MockOclocArgHelper::FilesMap files{{"src.gen", MockOclocArgHelper::FileData(reinterpret_cast<const char *>(zebin.storage.data()),
                                                                                reinterpret_cast<const char *>(zebin.storage.data()) + zebin.storage.size())}};
    MockOclocArgHelper argHelper{files};
    argHelper.getPrinterRef().setSuppressMessages(true);
    int res = Ocloc::validate({"-file", "src.gen"}, &argHelper);
    std::string oclocStdout = argHelper.getPrinterRef().getLog().str();
    EXPECT_EQ(0, res) << oclocStdout;
}

TEST(OclocValidate, WhenInputIsValid32BitZebinThenReturnSucceed) {
    ZebinTestData::ValidEmptyProgram<NEO::Elf::EI_CLASS_32> zebin;
    MockOclocArgHelper::FilesMap files{{"src.gen", MockOclocArgHelper::FileData(reinterpret_cast<const char *>(zebin.storage.data()),
                                                                                reinterpret_cast<const char *>(zebin.storage.data()) + zebin.storage.size())}};
    MockOclocArgHelper argHelper{files};
    argHelper.getPrinterRef().setSuppressMessages(true);
    int res = Ocloc::validate({"-file", "src.gen"}, &argHelper);
    std::string oclocStdout = argHelper.getPrinterRef().getLog().str();
    EXPECT_EQ(0, res) << oclocStdout;
}

TEST(OclocValidate, WhenWarningsEmitedThenRedirectsThemToStdout) {
    ZebinTestData::ValidEmptyProgram zebin;
    NEO::ConstStringRef miscData{"Miscellaneous data"};
    zebin.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_MISC, ".misc.other", ArrayRef<const uint8_t>::fromAny(miscData.begin(), miscData.size()));
    MockOclocArgHelper::FilesMap files{{"src.gen", MockOclocArgHelper::FileData(reinterpret_cast<const char *>(zebin.storage.data()),
                                                                                reinterpret_cast<const char *>(zebin.storage.data()) + zebin.storage.size())}};
    MockOclocArgHelper argHelper{files};
    argHelper.getPrinterRef().setSuppressMessages(true);
    int res = Ocloc::validate({"-file", "src.gen"}, &argHelper);
    std::string oclocStdout = argHelper.getPrinterRef().getLog().str();
    EXPECT_EQ(0, res) << oclocStdout;
    EXPECT_NE(nullptr, strstr(oclocStdout.c_str(), "Validator detected potential problems :\nDeviceBinaryFormat::zebin : unhandled SHT_ZEBIN_MISC section : .misc.other currently supports only : .misc.buildOptions.")) << oclocStdout;
}

TEST(OclocValidate, WhenErrorsEmitedThenRedirectsThemToStdout) {
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Zebin::Elf::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);
    std::string zeInfo = "version:" + versionToString(NEO::Zebin::ZeInfo::zeInfoDecoderVersion) + "\nkernels : \nkernels :\n";
    zebin.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const char>(zeInfo).toArrayRef<const uint8_t>());
    MockOclocArgHelper::FilesMap files{{"src.gen", MockOclocArgHelper::FileData(reinterpret_cast<const char *>(zebin.storage.data()),
                                                                                reinterpret_cast<const char *>(zebin.storage.data()) + zebin.storage.size())}};
    MockOclocArgHelper argHelper{files};
    argHelper.getPrinterRef().setSuppressMessages(true);
    int res = Ocloc::validate({"-file", "src.gen"}, &argHelper);
    std::string oclocStdout = argHelper.getPrinterRef().getLog().str();
    EXPECT_EQ(static_cast<int>(NEO::DecodeError::invalidBinary), res) << oclocStdout;
    EXPECT_NE(nullptr, strstr(oclocStdout.c_str(), "Validator detected errors :\nDeviceBinaryFormat::zebin::ZeInfo : Expected at most 1 of kernels, got : 2")) << oclocStdout;
}
