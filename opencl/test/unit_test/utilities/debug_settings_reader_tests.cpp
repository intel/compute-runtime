/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/utilities/debug_settings_reader.h"

#include "opencl/source/os_interface/ocl_reg_path.h"
#include "test.h"

#include "gtest/gtest.h"

#include <fstream>
#include <string>

using namespace NEO;

TEST(SettingsReader, Create) {
    SettingsReader *reader = SettingsReader::create(oclRegPath);
    EXPECT_NE(nullptr, reader);
    delete reader;
}

TEST(SettingsReader, GivenNoSettingsFileWhenCreatingSettingsReaderThenOsReaderIsCreated) {
    remove(SettingsReader::settingsFileName);
    auto fileReader = std::unique_ptr<SettingsReader>(SettingsReader::createFileReader());
    EXPECT_EQ(nullptr, fileReader.get());

    auto osReader = std::unique_ptr<SettingsReader>(SettingsReader::create(oclRegPath));
    EXPECT_NE(nullptr, osReader.get());
}

TEST(SettingsReader, GivenSettingsFileExistsWhenCreatingSettingsReaderThenFileReaderIsCreated) {
    bool settingsFileExists = fileExists(SettingsReader::settingsFileName);
    if (!settingsFileExists) {
        const char data[] = "ProductFamilyOverride = test";
        writeDataToFile(SettingsReader::settingsFileName, &data, sizeof(data));
    }
    auto reader = std::unique_ptr<SettingsReader>(SettingsReader::create(oclRegPath));
    EXPECT_NE(nullptr, reader.get());
    std::string defaultValue("unk");
    EXPECT_STREQ("test", reader->getSetting("ProductFamilyOverride", defaultValue).c_str());

    std::remove(SettingsReader::settingsFileName);
}

TEST(SettingsReader, CreateFileReader) {
    bool settingsFileExists = fileExists(SettingsReader::settingsFileName);
    if (!settingsFileExists) {
        char data = 0;
        writeDataToFile(SettingsReader::settingsFileName, &data, 0);
    }
    SettingsReader *reader = SettingsReader::createFileReader();
    EXPECT_NE(nullptr, reader);

    if (!settingsFileExists) {
        remove(SettingsReader::settingsFileName);
    }
    delete reader;
}

TEST(SettingsReader, CreateOsReader) {
    SettingsReader *reader = SettingsReader::createOsReader(false, oclRegPath);
    EXPECT_NE(nullptr, reader);
    delete reader;
}

TEST(SettingsReader, CreateOsReaderWithRegKey) {
    std::string regKey = oclRegPath;
    std::unique_ptr<SettingsReader> reader(SettingsReader::createOsReader(false, regKey));
    EXPECT_NE(nullptr, reader);
}

TEST(SettingsReader, givenPrintDebugStringWhenCalledWithTrueItPrintsToOutput) {
    int i = 4;
    testing::internal::CaptureStdout();
    printDebugString(true, stdout, "testing error %d", i);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STRNE(output.c_str(), "");
}
TEST(SettingsReader, givenPrintDebugStringWhenCalledWithFalseThenNothingIsPrinted) {
    int i = 4;
    testing::internal::CaptureStdout();
    printDebugString(false, stderr, "Error String %d", i);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STREQ(output.c_str(), "");
}
