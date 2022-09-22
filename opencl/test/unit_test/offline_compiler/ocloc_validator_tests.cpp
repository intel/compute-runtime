/*
 * Copyright (C) 2020-2022 Intel Corporation
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
    argHelper.getPrinterRef() = MessagePrinter(true);
    int res = Ocloc::validate({}, &argHelper);
    EXPECT_EQ(-1, res);
    std::string oclocStdout = argHelper.getPrinterRef().getLog().str();
    EXPECT_STREQ("Error : Mandatory argument -file is missing.\n", oclocStdout.c_str());
}

TEST(OclocValidate, WhenInputFileIsMissingThenFail) {
    MockOclocArgHelper::FilesMap files;
    MockOclocArgHelper argHelper{files};
    argHelper.getPrinterRef() = MessagePrinter(true);
    int res = Ocloc::validate({"-file", "src.gen"}, &argHelper);
    EXPECT_EQ(-1, res);
    std::string oclocStdout = argHelper.getPrinterRef().getLog().str();
    EXPECT_STREQ("Error : Input file missing : src.gen\n", oclocStdout.c_str());
}

TEST(OclocValidate, WhenInputFileIsAvailableThenLogItsSize) {
    MockOclocArgHelper::FilesMap files{{"src.gen", "01234567"}};
    MockOclocArgHelper argHelper{files};
    argHelper.getPrinterRef() = MessagePrinter(true);
    int res = Ocloc::validate({"-file", "src.gen"}, &argHelper);
    EXPECT_NE(0, res);
    std::string oclocStdout = argHelper.getPrinterRef().getLog().str();
    EXPECT_NE(nullptr, strstr(oclocStdout.c_str(), "Validating : src.gen (8 bytes).\n")) << oclocStdout;
}

TEST(OclocValidate, WhenInputFileIsNotZebinThenFail) {
    MockOclocArgHelper::FilesMap files{{"src.gen", "01234567"}};
    MockOclocArgHelper argHelper{files};
    argHelper.getPrinterRef() = MessagePrinter(true);
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
    argHelper.getPrinterRef() = MessagePrinter(true);
    int res = Ocloc::validate({"-file", "src.gen"}, &argHelper);
    std::string oclocStdout = argHelper.getPrinterRef().getLog().str();
    EXPECT_EQ(0, res) << oclocStdout;
}

TEST(OclocValidate, WhenInputIsValid32BitZebinThenReturnSucceed) {
    ZebinTestData::ValidEmptyProgram<NEO::Elf::EI_CLASS_32> zebin;
    MockOclocArgHelper::FilesMap files{{"src.gen", MockOclocArgHelper::FileData(reinterpret_cast<const char *>(zebin.storage.data()),
                                                                                reinterpret_cast<const char *>(zebin.storage.data()) + zebin.storage.size())}};
    MockOclocArgHelper argHelper{files};
    argHelper.getPrinterRef() = MessagePrinter(true);
    int res = Ocloc::validate({"-file", "src.gen"}, &argHelper);
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
    int res = Ocloc::validate({"-file", "src.gen"}, &argHelper);
    std::string oclocStdout = argHelper.getPrinterRef().getLog().str();
    EXPECT_EQ(0, res) << oclocStdout;
    EXPECT_NE(nullptr, strstr(oclocStdout.c_str(), "Validator detected potential problems :\nDeviceBinaryFormat::Zebin : Expected at least one .ze_info section, got 0")) << oclocStdout;
}

TEST(OclocValidate, WhenErrorsEmitedThenRedirectsThemToStdout) {
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Elf::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo);
    std::string zeInfo = "version:" + versionToString(NEO::zeInfoDecoderVersion) + "\nkernels : \nkernels :\n";
    zebin.appendSection(NEO::Elf::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, ArrayRef<const char>(zeInfo).toArrayRef<const uint8_t>());
    MockOclocArgHelper::FilesMap files{{"src.gen", MockOclocArgHelper::FileData(reinterpret_cast<const char *>(zebin.storage.data()),
                                                                                reinterpret_cast<const char *>(zebin.storage.data()) + zebin.storage.size())}};
    MockOclocArgHelper argHelper{files};
    argHelper.getPrinterRef() = MessagePrinter(true);
    int res = Ocloc::validate({"-file", "src.gen"}, &argHelper);
    std::string oclocStdout = argHelper.getPrinterRef().getLog().str();
    EXPECT_EQ(static_cast<int>(NEO::DecodeError::InvalidBinary), res) << oclocStdout;
    EXPECT_NE(nullptr, strstr(oclocStdout.c_str(), "Validator detected errors :\nNEO::Yaml : Could not parse line : [1] : [kernels ] <-- parser position on error. Reason : Vector data type expects to have at least one value starting with -")) << oclocStdout;
}

TEST(OclocValidate, givenDeviceProductTableEveryProductMatchesProperPattern) {
    MockOclocArgHelper::FilesMap files{{"src.gen", "01234567"}};
    MockOclocArgHelper argHelper{files};
    ASSERT_GE(argHelper.deviceProductTable.size(), 1u);
    std::vector<std::string> genPatterns;
    for (int j = 0; j < IGFX_MAX_PRODUCT; j++) {
        if (NEO::hardwarePrefix[j] == nullptr)
            continue;
        genPatterns.push_back(NEO::hardwarePrefix[j]);
    }
    ASSERT_GE(genPatterns.size(), 1u);
    if (argHelper.deviceProductTable.size() == 1 && argHelper.deviceProductTable[0].deviceId == 0) {
        auto &deviceProductTable = const_cast<std::vector<DeviceProduct> &>(argHelper.deviceProductTable);
        deviceProductTable[0].product = genPatterns[0];
        deviceProductTable[0].deviceId = 0x123;
        deviceProductTable.push_back(DeviceProduct{0, ""});
    }
    for (int i = 0; argHelper.deviceProductTable[i].deviceId != 0; i++) {
        auto res = std::find(genPatterns.begin(), genPatterns.end(), argHelper.returnProductNameForDevice(argHelper.deviceProductTable[i].deviceId));
        EXPECT_NE(res, genPatterns.end());
    }
}