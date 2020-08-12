/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_validator.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/test/unit_test/device_binary_format/zebin_tests.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_argument_helper.h"

#include "gtest/gtest.h"

PRODUCT_FAMILY productFamily;

TEST(OclocValidate, WhenFileArgIsMissingThenFail) {
    std::map<std::string, std::string> files;
    MockOclocArgHelper argHelper{files};
    argHelper.getPrinterRef() = MessagePrinter(true);
    int res = NEO::Ocloc::validate({}, &argHelper);
    EXPECT_EQ(-1, res);
    std::string oclocStdout = argHelper.getPrinterRef().getLog().str();
    EXPECT_STREQ("Error : Mandatory argument -file is missing.\n", oclocStdout.c_str());
}

TEST(OclocValidate, WhenInputFileIsMissingThenFail) {
    MockOclocArgHelper::FilesMap files;
    MockOclocArgHelper argHelper{files};
    argHelper.getPrinterRef() = MessagePrinter(true);
    int res = NEO::Ocloc::validate({"-file", "src.gen"}, &argHelper);
    EXPECT_EQ(-1, res);
    std::string oclocStdout = argHelper.getPrinterRef().getLog().str();
    EXPECT_STREQ("Error : Input file missing : src.gen\n", oclocStdout.c_str());
}

TEST(OclocValidate, WhenInputFileIsAvailableTheLogItsSize) {
    MockOclocArgHelper::FilesMap files{{"src.gen", "01234567"}};
    MockOclocArgHelper argHelper{files};
    argHelper.getPrinterRef() = MessagePrinter(true);
    int res = NEO::Ocloc::validate({"-file", "src.gen"}, &argHelper);
    EXPECT_NE(0, res);
    std::string oclocStdout = argHelper.getPrinterRef().getLog().str();
    EXPECT_NE(nullptr, strstr(oclocStdout.c_str(), "Validating : src.gen (8 bytes).\n")) << oclocStdout;
}

TEST(OclocValidate, WhenInputFileIsNotZebinThenFail) {
    MockOclocArgHelper::FilesMap files{{"src.gen", "01234567"}};
    MockOclocArgHelper argHelper{files};
    argHelper.getPrinterRef() = MessagePrinter(true);
    int res = NEO::Ocloc::validate({"-file", "src.gen"}, &argHelper);
    EXPECT_EQ(-2, res);
    std::string oclocStdout = argHelper.getPrinterRef().getLog().str();
    EXPECT_NE(nullptr, strstr(oclocStdout.c_str(), "Input is not a Zebin file (not elf or wrong elf object file type)")) << oclocStdout;
}

TEST(OclocValidate, WhenInputIsValidZebinThenReturnSucceed) {
    ZebinTestData::ValidEmptyProgram zebin;
    MockOclocArgHelper::FilesMap files{{"src.gen", MockOclocArgHelper::FileData(reinterpret_cast<const char *>(zebin.storage.data()),
                                                                                reinterpret_cast<const char *>(zebin.storage.data()) + zebin.storage.size())}};
    MockOclocArgHelper argHelper{files};
    argHelper.getPrinterRef() = MessagePrinter(true);
    int res = NEO::Ocloc::validate({"-file", "src.gen"}, &argHelper);
    std::string oclocStdout = argHelper.getPrinterRef().getLog().str();
    EXPECT_EQ(0, res) << oclocStdout;
}

TEST(OclocValidate, WhenWarningsEmitedThenRedirectsThemToStdout) {
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Elf::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo);
    MockOclocArgHelper::FilesMap files{{"src.gen", MockOclocArgHelper::FileData(reinterpret_cast<const char *>(zebin.storage.data()),
                                                                                reinterpret_cast<const char *>(zebin.storage.data()) + zebin.storage.size())}};
    MockOclocArgHelper argHelper{files};
    argHelper.getPrinterRef() = MessagePrinter(true);
    int res = NEO::Ocloc::validate({"-file", "src.gen"}, &argHelper);
    std::string oclocStdout = argHelper.getPrinterRef().getLog().str();
    EXPECT_EQ(0, res) << oclocStdout;
    EXPECT_NE(nullptr, strstr(oclocStdout.c_str(), "Validator detected potential problems :\nDeviceBinaryFormat::Zebin : Expected at least one .ze_info section, got 0")) << oclocStdout;
}

TEST(OclocValidate, WhenErrorsEmitedThenRedirectsThemToStdout) {
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Elf::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo);
    zebin.appendSection(NEO::Elf::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, ArrayRef<const char>("kernels : \nkernels :\n").toArrayRef<const uint8_t>());
    MockOclocArgHelper::FilesMap files{{"src.gen", MockOclocArgHelper::FileData(reinterpret_cast<const char *>(zebin.storage.data()),
                                                                                reinterpret_cast<const char *>(zebin.storage.data()) + zebin.storage.size())}};
    MockOclocArgHelper argHelper{files};
    argHelper.getPrinterRef() = MessagePrinter(true);
    int res = NEO::Ocloc::validate({"-file", "src.gen"}, &argHelper);
    std::string oclocStdout = argHelper.getPrinterRef().getLog().str();
    EXPECT_EQ(static_cast<int>(NEO::DecodeError::InvalidBinary), res) << oclocStdout;
    EXPECT_NE(nullptr, strstr(oclocStdout.c_str(), "Validator detected errors :\nDeviceBinaryFormat::Zebin::.ze_info : Expected at most one kernels entry in global scope of .ze_info, got : 2")) << oclocStdout;
}
