/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/utilities/debug_file_reader.h"
#include "shared/source/utilities/debug_settings_reader.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

#include <fstream>
#include <string>

using namespace NEO;

class MockSettingsReader : public SettingsReader {
  public:
    std::string getSetting(const char *settingName, const std::string &value, DebugVarPrefix &type) override {
        return value;
    }
    std::string getSetting(const char *settingName, const std::string &value) override {
        return value;
    }
    bool getSetting(const char *settingName, bool defaultValue, DebugVarPrefix &type) override { return defaultValue; };
    bool getSetting(const char *settingName, bool defaultValue) override { return defaultValue; };
    int64_t getSetting(const char *settingName, int64_t defaultValue, DebugVarPrefix &type) override { return defaultValue; };
    int64_t getSetting(const char *settingName, int64_t defaultValue) override { return defaultValue; };
    int32_t getSetting(const char *settingName, int32_t defaultValue, DebugVarPrefix &type) override { return defaultValue; };
    int32_t getSetting(const char *settingName, int32_t defaultValue) override { return defaultValue; };
    const char *appSpecificLocation(const std::string &name) override { return name.c_str(); };
};

namespace SettingsReaderTests {

void writeDataToFile(const char *filename, const void *pData, size_t dataSize) {
    std::ofstream file;
    file.open(filename);
    file.write(static_cast<const char *>(pData), dataSize);
    file.close();
}

TEST(SettingsReader, WhenCreatingSettingsReaderThenReaderIsCreated) {
    auto reader = std::unique_ptr<SettingsReader>(SettingsReader::create(ApiSpecificConfig::getRegistryPath()));
    EXPECT_NE(nullptr, reader.get());
}

TEST(SettingsReader, GivenNoSettingsFileWhenCreatingSettingsReaderThenOsReaderIsCreated) {
    ASSERT_FALSE(fileExists(SettingsReader::settingsFileName));

    auto fileReader = std::unique_ptr<SettingsReader>(SettingsReader::createFileReader());
    EXPECT_EQ(nullptr, fileReader.get());

    auto osReader = std::unique_ptr<SettingsReader>(SettingsReader::create(ApiSpecificConfig::getRegistryPath()));
    EXPECT_NE(nullptr, osReader.get());
}

TEST(SettingsReader, GivenSettingsFileExistsWhenCreatingSettingsReaderThenReaderIsCreated) {
    ASSERT_FALSE(fileExists(SettingsReader::settingsFileName));

    const char data[] = "ProductFamilyOverride = test";
    writeDataToFile(SettingsReader::settingsFileName, &data, sizeof(data));

    auto reader = std::unique_ptr<SettingsReader>(SettingsReader::create(ApiSpecificConfig::getRegistryPath()));
    EXPECT_NE(nullptr, reader.get());
    std::string defaultValue("unk");
    EXPECT_STREQ("test", reader->getSetting("ProductFamilyOverride", defaultValue).c_str());

    std::remove(SettingsReader::settingsFileName);
}

TEST(SettingsReader, WhenCreatingFileReaderThenReaderIsCreated) {
    ASSERT_FALSE(fileExists(SettingsReader::settingsFileName));
    char data = 0;
    writeDataToFile(SettingsReader::settingsFileName, &data, 0);

    auto reader = std::unique_ptr<SettingsReader>(SettingsReader::createFileReader());
    EXPECT_NE(nullptr, reader.get());

    std::remove(SettingsReader::settingsFileName);
}

TEST(SettingsReader, WhenCreatingFileReaderUseNeoFileIfNoDefault) {
    ASSERT_FALSE(fileExists(SettingsReader::settingsFileName));
    ASSERT_FALSE(fileExists(SettingsReader::neoSettingsFileName));
    char data = 0;
    writeDataToFile(SettingsReader::neoSettingsFileName, &data, 0);

    auto reader = std::unique_ptr<SettingsReader>(SettingsReader::createFileReader());
    EXPECT_NE(nullptr, reader.get());

    std::remove(SettingsReader::neoSettingsFileName);
}

TEST(SettingsReader, WhenCreatingOsReaderThenReaderIsCreated) {
    auto reader = std::unique_ptr<SettingsReader>(SettingsReader::createOsReader(false, ApiSpecificConfig::getRegistryPath()));
    EXPECT_NE(nullptr, reader.get());
}

TEST(SettingsReader, GivenRegKeyWhenCreatingOsReaderThenReaderIsCreated) {
    std::string regKey = ApiSpecificConfig::getRegistryPath();
    auto reader = std::unique_ptr<SettingsReader>(SettingsReader::createOsReader(false, regKey));
    EXPECT_NE(nullptr, reader.get());
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
} // namespace SettingsReaderTests