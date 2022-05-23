/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/debug_env_reader.h"

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/test_macros/test.h"

#include <memory>
#include <unordered_map>

namespace NEO {

class DebugEnvReaderTests : public ::testing::Test {
  public:
    void SetUp() override {
        evr = SettingsReader::createOsReader(false, "");
        EXPECT_NE(nullptr, evr);
    }
    void TearDown() override {
        delete evr;
    }
    SettingsReader *evr = nullptr;
};

TEST_F(DebugEnvReaderTests, GivenSetVariableThenSetValueIsReturned) {
    int32_t ret;
    std::string retString;
    std::string defaultString = "Default Value";
    std::string setString = "Expected Value";
    const char *testingVariableName = "TestingVariable";
    const char *testingVariableValue = "1234";

    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{testingVariableName, testingVariableValue}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

        ret = evr->getSetting(testingVariableName, 1);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(1234, ret);
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{testingVariableName, setString.c_str()}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

        retString = evr->getSetting("TestingVariable", defaultString);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(0, retString.compare(setString));
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

        ret = evr->getSetting("TestingVariable", 1);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(1, ret);
    }
}

TEST_F(DebugEnvReaderTests, GivenUnsetVariableThenDefaultValueIsReturned) {
    int32_t ret;
    std::string retString;
    std::string defaultString = "Default Value";

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    ret = evr->getSetting("TestingVariable", 1);
    EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
    EXPECT_EQ(1, ret);

    retString = evr->getSetting("TestingVariable", defaultString);
    EXPECT_EQ(2u, IoFunctions::mockGetenvCalled);
    EXPECT_EQ(0, retString.compare(defaultString));
}

TEST_F(DebugEnvReaderTests, GivenBoolEnvVariableWhenGettingThenCorrectValueIsReturned) {
    bool ret;
    bool defaultValue = true;
    bool expectedValue = false;

    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{"TestingVariable", "0"}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

        ret = evr->getSetting("TestingVariable", defaultValue);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(expectedValue, ret);
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

        ret = evr->getSetting("TestingVariable", defaultValue);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(defaultValue, ret);
    }
}

TEST_F(DebugEnvReaderTests, WhenSettingAppSpecificLocationThenLocationIsReturned) {
    std::string appSpecific;
    appSpecific = "cl_cache_dir";
    EXPECT_EQ(appSpecific, evr->appSpecificLocation(appSpecific));
}

TEST_F(DebugEnvReaderTests, givenEnvironmentVariableReaderWhenCreateOsReaderWithStringThenNotNullPointer) {
    std::unique_ptr<SettingsReader> evr(SettingsReader::createOsReader(false, ""));
    EXPECT_NE(nullptr, evr);
}
} // namespace NEO
