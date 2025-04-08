/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/utilities/debug_file_reader.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

#include <memory>
#include <string>

namespace NEO {
extern ApiSpecificConfig::ApiType apiTypeForUlts;
} // namespace NEO

using namespace NEO;

class TestSettingsFileReader : public SettingsFileReader {
  public:
    using SettingsFileReader::parseStream;

    TestSettingsFileReader() : TestSettingsFileReader(SettingsReader::settingsFileName) {
    }
    TestSettingsFileReader(const char *filePath) : SettingsFileReader(filePath) {
    }

    ~TestSettingsFileReader() override {
    }

    bool hasSetting(const char *settingName) {
        std::map<std::string, std::string>::iterator it = settingStringMap.find(std::string(settingName));
        return (it != settingStringMap.end());
    }

    size_t getStringSettingsCount() {
        return settingStringMap.size();
    }

    static const std::string getTestPath() {
        return clFiles + "igdrcl.config";
    }
    static const std::string getStringTestPath() {
        return clFiles + "igdrcl_string.config";
    }

    static const std::string getNeoStringTestPath() {
        return clFiles + "neo_string.config";
    }
};

TEST(SettingsFileReader, GivenFilesDoesNotExistWhenCreatingFileReaderThenCreationSucceeds) {
    bool settingsFileExists = fileExists(SettingsReader::settingsFileName);

    // if settings file exists, remove it
    if (settingsFileExists) {
        remove(SettingsReader::settingsFileName);
    }

    // Use current location for file read
    auto reader = std::make_unique<TestSettingsFileReader>();
    ASSERT_NE(nullptr, reader);

    EXPECT_EQ(0u, reader->getStringSettingsCount());
}

TEST(SettingsFileReader, WhenGettingSettingThenCorrectStringValueIsReturned) {
    // Use test settings file
    auto reader = std::make_unique<TestSettingsFileReader>(TestSettingsFileReader::getStringTestPath().c_str());
    ASSERT_NE(nullptr, reader);

    std::string retValue;
    // StringTestKey is defined in file: unit_tests\helpers\test_debug_variables.inl
    std::string returnedStringValue = reader->getSetting("StringTestKey", retValue);

    // "Test Value" is a value that should be read from file defined in stringTestPath member
    EXPECT_STREQ(returnedStringValue.c_str(), "TestValue");

#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description) \
    {                                                                             \
        dataType defaultData = defaultValue;                                      \
        dataType tempData = reader->getSetting(#variableName, defaultData);       \
        if (tempData == defaultData) {                                            \
            EXPECT_TRUE(true);                                                    \
        }                                                                         \
    }
#define DECLARE_DEBUG_SCOPED_V(dataType, variableName, defaultValue, description, ...) \
    DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)
#include "shared/test/unit_test/helpers/test_debug_variables.inl"
#undef DECLARE_DEBUG_SCOPED_V
#undef DECLARE_DEBUG_VARIABLE
}

TEST(SettingsFileReader, givenDebugFileSettingInWhichStringIsFollowedByIntegerWhenItIsParsedThenProperValuesAreObtained) {
    auto reader = std::make_unique<TestSettingsFileReader>(TestSettingsFileReader::getStringTestPath().c_str());
    ASSERT_NE(nullptr, reader.get());

    int32_t retValue = 0;
    int32_t returnedIntValue = reader->getSetting("IntTestKey", retValue);

    EXPECT_EQ(123, returnedIntValue);

    int32_t returnedIntValueHex = reader->getSetting("IntTestKeyHex", 0);

    EXPECT_EQ(0xABCD, returnedIntValueHex);

    std::string retValueString;
    std::string returnedStringValue = reader->getSetting("StringTestKey", retValueString);

    EXPECT_STREQ(returnedStringValue.c_str(), "TestValue");
}

TEST(SettingsFileReader, GivenSettingNotInFileWhenGettingSettingThenProvidedDefaultIsReturned) {

    // Use test settings file
    auto reader = std::make_unique<TestSettingsFileReader>(TestSettingsFileReader::getTestPath().c_str());
    ASSERT_NE(nullptr, reader);

    bool defaultBoolValue = false;
    bool returnedBoolValue = reader->getSetting("BoolSettingNotExistingInFile", defaultBoolValue);

    EXPECT_EQ(defaultBoolValue, returnedBoolValue);

    int32_t defaultIntValue = 123;
    int32_t returnedIntValue = reader->getSetting("IntSettingNotExistingInFile", defaultIntValue);

    EXPECT_EQ(defaultIntValue, returnedIntValue);

    std::string defaultStringValue = "ABCD";
    std::string returnedStringValue = reader->getSetting("StringSettingNotExistingInFile", defaultStringValue);

    EXPECT_EQ(defaultStringValue, returnedStringValue);
}

TEST(SettingsFileReader, WhenGettingAppSpecificLocationThenCorrectLocationIsReturned) {
    std::unique_ptr<TestSettingsFileReader> reader(new TestSettingsFileReader(TestSettingsFileReader::getTestPath().c_str()));
    std::string appSpecific = "cl_cache_dir";
    EXPECT_EQ(appSpecific, reader->appSpecificLocation(appSpecific));
}

TEST(SettingsFileReader, givenHexNumbersSemiColonSeparatedListInInputStreamWhenParsingThenCorrectStringValueIsStored) {
    auto reader = std::make_unique<TestSettingsFileReader>();
    ASSERT_NE(nullptr, reader);

    // No settings should be parsed initially
    EXPECT_EQ(0u, reader->getStringSettingsCount());

    std::stringstream inputLineWithSemiColonList("KeyName = 0x1234;0x5555");

    reader->parseStream(inputLineWithSemiColonList);

    std::string defaultStringValue = "FailedToParse";
    std::string returnedStringValue = reader->getSetting("KeyName", defaultStringValue);

    EXPECT_STREQ("0x1234;0x5555", returnedStringValue.c_str());
}

TEST(SettingsFileReader, given64bitKeyValueWhenGetSettingThenValueIsCorrect) {
    auto reader = std::make_unique<TestSettingsFileReader>();
    ASSERT_NE(nullptr, reader);

    EXPECT_EQ(0u, reader->getStringSettingsCount());
    std::stringstream inputLine("Example64BitKey = -18764712120594");
    reader->parseStream(inputLine);

    int64_t defaultValue = 0;
    int64_t returnedValue = reader->getSetting("Example64BitKey", defaultValue);

    EXPECT_EQ(-18764712120594, returnedValue);
}

TEST(SettingsFileReader, givenKeyValueWithoutSpacesWhenGetSettingThenValueIsCorrect) {
    auto reader = std::make_unique<TestSettingsFileReader>();
    ASSERT_NE(nullptr, reader);
    EXPECT_EQ(0u, reader->getStringSettingsCount());

    std::stringstream inputLine("SomeKey=12");
    reader->parseStream(inputLine);

    int64_t returnedValue = reader->getSetting("SomeKey", 0);
    EXPECT_EQ(1u, reader->getStringSettingsCount());
    EXPECT_EQ(12, returnedValue);
}

TEST(SettingsFileReader, givenKeyValueWithAdditionalWhitespaceCharactersWhenGetSettingThenValueIsCorrect) {
    auto reader = std::make_unique<TestSettingsFileReader>();
    ASSERT_NE(nullptr, reader);
    EXPECT_EQ(0u, reader->getStringSettingsCount());

    std::stringstream inputLine("\t \t SomeKey\t \t =\t \t 12\t \t ");
    reader->parseStream(inputLine);

    int64_t returnedValue = reader->getSetting("SomeKey", 0);
    EXPECT_EQ(1u, reader->getStringSettingsCount());
    EXPECT_EQ(12, returnedValue);
}

TEST(SettingsFileReader, givenKeyValueWithAdditionalCharactersWhenGetSettingThenValueIsIncorrect) {
    {
        auto reader = std::make_unique<TestSettingsFileReader>();
        ASSERT_NE(nullptr, reader);
        EXPECT_EQ(0u, reader->getStringSettingsCount());

        std::stringstream inputLine("Some Key = 12");
        reader->parseStream(inputLine);

        EXPECT_EQ(0u, reader->getStringSettingsCount());
    }
    {
        auto reader = std::make_unique<TestSettingsFileReader>();
        ASSERT_NE(nullptr, reader);
        EXPECT_EQ(0u, reader->getStringSettingsCount());

        std::stringstream inputLine("SomeKey = 1 2");
        reader->parseStream(inputLine);

        EXPECT_EQ(0u, reader->getStringSettingsCount());
    }
}

TEST(SettingsFileReader, givenMultipleKeysWhenGetSettingThenInvalidKeysAreSkipped) {
    auto reader = std::make_unique<TestSettingsFileReader>();
    ASSERT_NE(nullptr, reader);
    EXPECT_EQ(0u, reader->getStringSettingsCount());

    std::string testFile;
    testFile.append("InvalidKey1 = 1 2\n");
    testFile.append("ValidKey1 = 12\n");
    testFile.append("InvalidKey2 = - 1\n");
    testFile.append("ValidKey2 = 128\n");
    std::stringstream inputFile(testFile);
    reader->parseStream(inputFile);

    EXPECT_EQ(2u, reader->getStringSettingsCount());
    EXPECT_EQ(0, reader->getSetting("InvalidKey1", 0));
    EXPECT_EQ(0, reader->getSetting("InvalidKey2", 0));
    EXPECT_EQ(12, reader->getSetting("ValidKey1", 0));
    EXPECT_EQ(128, reader->getSetting("ValidKey2", 0));
}

TEST(SettingsFileReader, givenNoKeyOrNoValueWhenGetSettingThenExceptionIsNotThrown) {
    {
        auto reader = std::make_unique<TestSettingsFileReader>();
        ASSERT_NE(nullptr, reader);
        EXPECT_EQ(0u, reader->getStringSettingsCount());

        std::stringstream inputLine("= 12");
        EXPECT_NO_THROW(reader->parseStream(inputLine));

        EXPECT_EQ(0u, reader->getStringSettingsCount());
    }
    {
        auto reader = std::make_unique<TestSettingsFileReader>();
        ASSERT_NE(nullptr, reader);
        EXPECT_EQ(0u, reader->getStringSettingsCount());

        std::stringstream inputLine("SomeKey =");
        EXPECT_NO_THROW(reader->parseStream(inputLine));

        EXPECT_EQ(0u, reader->getStringSettingsCount());
    }
    {
        auto reader = std::make_unique<TestSettingsFileReader>();
        ASSERT_NE(nullptr, reader);
        EXPECT_EQ(0u, reader->getStringSettingsCount());

        std::stringstream inputLine("=");
        EXPECT_NO_THROW(reader->parseStream(inputLine));

        EXPECT_EQ(0u, reader->getStringSettingsCount());
    }
}

TEST(SettingsFileReader, givenPrefixFileReadCorrectValueReturned) {
    auto reader = std::make_unique<TestSettingsFileReader>(TestSettingsFileReader::getStringTestPath().c_str());
    ASSERT_NE(nullptr, reader.get());
    VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::OCL);
    DebugVarPrefix type = DebugVarPrefix::none;
    int32_t retValue = 0;
    int32_t returnedIntValue = reader->getSetting("IntTestKey", retValue, type);
    EXPECT_EQ(DebugVarPrefix::none, type);
    EXPECT_EQ(123, returnedIntValue);

    int32_t returnedIntValueHex = reader->getSetting("IntTestKeyHex", 0, type);
    EXPECT_EQ(DebugVarPrefix::none, type);
    EXPECT_EQ(0xABCD, returnedIntValueHex);

    std::string retValueString = "";
    std::string returnedStringValue = reader->getSetting("StringTestKey", retValueString, type);
    EXPECT_EQ(DebugVarPrefix::none, type);
    EXPECT_STREQ(returnedStringValue.c_str(), "TestValue");

    auto neoReader = std::make_unique<TestSettingsFileReader>(TestSettingsFileReader::getNeoStringTestPath().c_str());
    ASSERT_NE(nullptr, neoReader.get());

    retValue = 0;
    returnedIntValue = neoReader->getSetting("IntTestKey", retValue, type);
    EXPECT_EQ(DebugVarPrefix::neo, type);
    EXPECT_EQ(123, returnedIntValue);

    returnedIntValueHex = neoReader->getSetting("IntTestKeyHex", 0, type);
    EXPECT_EQ(DebugVarPrefix::neo, type);
    EXPECT_EQ(0xABCD, returnedIntValueHex);

    retValueString = "";
    returnedStringValue = neoReader->getSetting("StringTestKey", retValueString, type);
    EXPECT_EQ(DebugVarPrefix::neo, type);
    EXPECT_STREQ(returnedStringValue.c_str(), "TestValue");
}
