/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/utilities/debug_settings_reader.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

#include <fstream>
#include <string>

using namespace NEO;

class MockSettingsReader : public SettingsReader {
  public:
    std::string getSetting(const char *settingName, const std::string &value) override {
        return value;
    }
    bool getSetting(const char *settingName, bool defaultValue) override { return defaultValue; };
    int64_t getSetting(const char *settingName, int64_t defaultValue) override { return defaultValue; };
    int32_t getSetting(const char *settingName, int32_t defaultValue) override { return defaultValue; };
    const char *appSpecificLocation(const std::string &name) override { return name.c_str(); };
};

TEST(SettingsReader, WhenCreatingSettingsReaderThenReaderIsCreated) {
    SettingsReader *reader = SettingsReader::create(ApiSpecificConfig::getRegistryPath());
    EXPECT_NE(nullptr, reader);
    delete reader;
}

TEST(SettingsReader, GivenNoSettingsFileWhenCreatingSettingsReaderThenOsReaderIsCreated) {
    remove(SettingsReader::settingsFileName);
    auto fileReader = std::unique_ptr<SettingsReader>(SettingsReader::createFileReader());
    EXPECT_EQ(nullptr, fileReader.get());

    auto osReader = std::unique_ptr<SettingsReader>(SettingsReader::create(ApiSpecificConfig::getRegistryPath()));
    EXPECT_NE(nullptr, osReader.get());
}

TEST(SettingsReader, GivenSettingsFileExistsWhenCreatingSettingsReaderThenReaderIsCreated) {
    bool settingsFileExists = fileExists(SettingsReader::settingsFileName);
    if (!settingsFileExists) {
        const char data[] = "ProductFamilyOverride = test";
        writeDataToFile(SettingsReader::settingsFileName, &data, sizeof(data));
    }
    auto reader = std::unique_ptr<SettingsReader>(SettingsReader::create(ApiSpecificConfig::getRegistryPath()));
    EXPECT_NE(nullptr, reader.get());
    std::string defaultValue("unk");
    EXPECT_STREQ("test", reader->getSetting("ProductFamilyOverride", defaultValue).c_str());

    std::remove(SettingsReader::settingsFileName);
}

TEST(SettingsReader, WhenCreatingFileReaderThenReaderIsCreated) {
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

TEST(SettingsReader, WhenCreatingOsReaderThenReaderIsCreated) {
    SettingsReader *reader = SettingsReader::createOsReader(false, ApiSpecificConfig::getRegistryPath());
    EXPECT_NE(nullptr, reader);
    delete reader;
}

TEST(SettingsReader, GivenRegKeyWhenCreatingOsReaderThenReaderIsCreated) {
    std::string regKey = ApiSpecificConfig::getRegistryPath();
    std::unique_ptr<SettingsReader> reader(SettingsReader::createOsReader(false, regKey));
    EXPECT_NE(nullptr, reader);
}

TEST(SettingsReader, GivenTrueWhenPrintingDebugStringThenPrintsToOutput) {
    int i = 4;
    testing::internal::CaptureStdout();
    PRINT_DEBUG_STRING(true, stdout, "testing error %d", i);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STRNE(output.c_str(), "");
}

TEST(SettingsReader, GivenFalseWhenPrintingDebugStringThenNoOutput) {
    int i = 4;
    testing::internal::CaptureStdout();
    PRINT_DEBUG_STRING(false, stderr, "Error String %d", i);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STREQ(output.c_str(), "");
}

TEST(SettingsReader, givenNonExistingEnvVarWhenGettingEnvThenNullptrIsReturned) {
    MockSettingsReader reader;
    auto value = reader.getenv("ThisEnvVarDoesNotExist");
    EXPECT_EQ(nullptr, value);
}
