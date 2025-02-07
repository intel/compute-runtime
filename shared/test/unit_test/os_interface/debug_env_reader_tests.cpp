/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/os_interface/debug_env_reader.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/test_macros/test.h"

#include <memory>
#include <unordered_map>

namespace NEO {

extern ApiSpecificConfig::ApiType apiTypeForUlts;

class DebugEnvReaderTests : public ::testing::Test {
  public:
    void SetUp() override {
        environmentVariableReader = std::make_unique<EnvironmentVariableReader>();
        EXPECT_NE(nullptr, environmentVariableReader);
    }
    void TearDown() override {
    }
    std::unique_ptr<EnvironmentVariableReader> environmentVariableReader;
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

        ret = environmentVariableReader->getSetting(testingVariableName, 1);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(1234, ret);
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{testingVariableName, setString.c_str()}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

        retString = environmentVariableReader->getSetting("TestingVariable", defaultString);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(0, retString.compare(setString));
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

        ret = environmentVariableReader->getSetting("TestingVariable", 1);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(1, ret);
    }
}

TEST_F(DebugEnvReaderTests, GivenSetVariableWithNoPrefixThenSetValueIsReturned) {
    int32_t ret;
    std::string retString;
    std::string defaultString = "Default Value";
    std::string setString = "Expected Value";
    const char *testingVariableName = "TestingVariable";
    const char *testingVariableValue = "1234";
    bool defaultBoolValue = true;
    VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{testingVariableName, testingVariableValue}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::none;

        ret = environmentVariableReader->getSetting(testingVariableName, 1, type);
        EXPECT_EQ(3u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::none, type);
        EXPECT_EQ(1234, ret);
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{testingVariableName, setString.c_str()}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::none;

        retString = environmentVariableReader->getSetting("TestingVariable", defaultString, type);
        EXPECT_EQ(3u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::none, type);
        EXPECT_EQ(0, retString.compare(setString));
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{testingVariableName, "0"}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::none;

        ret = environmentVariableReader->getSetting(testingVariableName, defaultBoolValue, type);
        EXPECT_EQ(3u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::none, type);
        EXPECT_EQ(0, ret);
    }
}

TEST_F(DebugEnvReaderTests, GivenSetVariableWithNeoPrefixThenSetValueIsReturned) {
    int32_t ret;
    std::string retString;
    std::string defaultString = "Default Value";
    std::string setString = "Expected Value";
    const char *testingVariableName = "TestingVariable";
    const char *neoTestingVariableName = "NEO_TestingVariable";
    const char *testingVariableValue = "1234";
    bool defaultBoolValue = true;
    VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{neoTestingVariableName, testingVariableValue}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::none;

        ret = environmentVariableReader->getSetting(testingVariableName, 1, type);
        EXPECT_EQ(2u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::neo, type);
        EXPECT_EQ(1234, ret);
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{neoTestingVariableName, setString.c_str()}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::none;

        retString = environmentVariableReader->getSetting("TestingVariable", defaultString, type);
        EXPECT_EQ(2u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::neo, type);
        EXPECT_EQ(0, retString.compare(setString));
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{neoTestingVariableName, "0"}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::none;

        ret = environmentVariableReader->getSetting(testingVariableName, defaultBoolValue, type);
        EXPECT_EQ(2u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::neo, type);
        EXPECT_EQ(0, ret);
    }
}

TEST_F(DebugEnvReaderTests, GivenSetVariableWithOclPrefixThenSetValueIsReturned) {
    int32_t ret;
    std::string retString;
    std::string defaultString = "Default Value";
    std::string setString = "Expected Value";
    const char *testingVariableName = "TestingVariable";
    const char *oclTestingVariableName = "NEO_OCL_TestingVariable";
    const char *testingVariableValue = "1234";
    bool defaultBoolValue = true;
    VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::OCL);
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{oclTestingVariableName, testingVariableValue}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::none;

        ret = environmentVariableReader->getSetting(testingVariableName, 1, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::neoOcl, type);
        EXPECT_EQ(1234, ret);
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{oclTestingVariableName, setString.c_str()}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::none;

        retString = environmentVariableReader->getSetting("TestingVariable", defaultString, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::neoOcl, type);
        EXPECT_EQ(0, retString.compare(setString));
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{oclTestingVariableName, "0"}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::none;

        ret = environmentVariableReader->getSetting(testingVariableName, defaultBoolValue, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::neoOcl, type);
        EXPECT_EQ(0, ret);
    }
}

TEST_F(DebugEnvReaderTests, GivenSetVariableWithL0PrefixThenSetValueIsReturned) {
    int32_t ret;
    std::string retString;
    std::string defaultString = "Default Value";
    std::string setString = "Expected Value";
    const char *testingVariableName = "TestingVariable";
    const char *lzeroTestingVariableName = "NEO_L0_TestingVariable";
    const char *testingVariableValue = "1234";
    bool defaultBoolValue = true;
    VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{lzeroTestingVariableName, testingVariableValue}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::none;
        ret = environmentVariableReader->getSetting(testingVariableName, 1, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::neoL0, type);
        EXPECT_EQ(1234, ret);
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{lzeroTestingVariableName, setString.c_str()}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::none;

        retString = environmentVariableReader->getSetting("TestingVariable", defaultString, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::neoL0, type);
        EXPECT_EQ(0, retString.compare(setString));
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{lzeroTestingVariableName, "0"}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::none;

        ret = environmentVariableReader->getSetting(testingVariableName, defaultBoolValue, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::neoL0, type);
        EXPECT_EQ(0, ret);
    }
}

TEST_F(DebugEnvReaderTests, GivenSetVariableWithL0MultiplePrefixesThenHighestPrioritySetValueIsReturned) {
    int32_t ret;
    std::string retString;
    std::string defaultString = "Default Value";
    std::string setString = "Expected Value";
    const char *testingVariableName = "TestingVariable";
    const char *neoTestingVariableName = "NEO_TestingVariable";
    const char *lzeroTestingVariableName = "NEO_L0_TestingVariable";
    const char *testingVariableValue = "1234";
    VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{testingVariableName, "1"},
                                                                     {neoTestingVariableName, "2"},
                                                                     {lzeroTestingVariableName, testingVariableValue}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::none;
        ret = environmentVariableReader->getSetting(testingVariableName, 1, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::neoL0, type);
        EXPECT_EQ(1234, ret);
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{testingVariableName, "1"},
                                                                     {neoTestingVariableName, "2"}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::none;
        ret = environmentVariableReader->getSetting(testingVariableName, 1, type);
        EXPECT_EQ(2u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::neo, type);
        EXPECT_EQ(2, ret);
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{testingVariableName, "Default None"},
                                                                     {neoTestingVariableName, "Default Neo"},
                                                                     {lzeroTestingVariableName, setString.c_str()}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::none;

        retString = environmentVariableReader->getSetting("TestingVariable", defaultString, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::neoL0, type);
        EXPECT_EQ(0, retString.compare(setString));
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{testingVariableName, "Default None"},
                                                                     {neoTestingVariableName, "Default Neo"}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::none;

        retString = environmentVariableReader->getSetting("TestingVariable", defaultString, type);
        EXPECT_EQ(2u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::neo, type);
        EXPECT_EQ(0, retString.compare("Default Neo"));
    }
}

TEST_F(DebugEnvReaderTests, GivenSetVariableWithOclMultipPrefixesThenHighestPrioritySetValueIsReturned) {
    int32_t ret;
    std::string retString;
    std::string defaultString = "Default Value";
    std::string setString = "Expected Value";
    const char *testingVariableName = "TestingVariable";
    const char *neoTestingVariableName = "NEO_TestingVariable";
    const char *oclTestingVariableName = "NEO_OCL_TestingVariable";
    const char *testingVariableValue = "1234";
    VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::OCL);
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{testingVariableName, "1"},
                                                                     {neoTestingVariableName, "2"},
                                                                     {oclTestingVariableName, testingVariableValue}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::none;
        ret = environmentVariableReader->getSetting(testingVariableName, 1, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::neoOcl, type);
        EXPECT_EQ(1234, ret);
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{testingVariableName, "1"},
                                                                     {neoTestingVariableName, "2"}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::none;
        ret = environmentVariableReader->getSetting(testingVariableName, 1, type);
        EXPECT_EQ(2u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::neo, type);
        EXPECT_EQ(2, ret);
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{testingVariableName, "Default None"},
                                                                     {neoTestingVariableName, "Default Neo"},
                                                                     {oclTestingVariableName, setString.c_str()}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::none;

        retString = environmentVariableReader->getSetting("TestingVariable", defaultString, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::neoOcl, type);
        EXPECT_EQ(0, retString.compare(setString));
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{testingVariableName, "Default None"},
                                                                     {neoTestingVariableName, "Default Neo"}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::none;

        retString = environmentVariableReader->getSetting("TestingVariable", defaultString, type);
        EXPECT_EQ(2u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::neo, type);
        EXPECT_EQ(0, retString.compare("Default Neo"));
    }
}

TEST_F(DebugEnvReaderTests, GivenSetVariableWithBothOclAndL0PrefixCorrectDriverValueReturned) {
    int32_t ret;
    std::string retString;
    std::string defaultString = "Default Value";
    std::string setString = "Expected Value";
    const char *testingVariableName = "TestingVariable";
    const char *lzeroTestingVariableName = "NEO_L0_TestingVariable";
    const char *oclTestingVariableName = "NEO_OCL_TestingVariable";
    const char *testingVariableValue = "1234";

    {
        VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::OCL);
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{lzeroTestingVariableName, "5678"},
                                                                     {oclTestingVariableName, testingVariableValue}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::none;
        ret = environmentVariableReader->getSetting(testingVariableName, 1, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::neoOcl, type);
        EXPECT_EQ(1234, ret);
    }
    {
        VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{lzeroTestingVariableName, "5678"},
                                                                     {oclTestingVariableName, testingVariableValue}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::none;
        ret = environmentVariableReader->getSetting(testingVariableName, 1, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::neoL0, type);
        EXPECT_EQ(5678, ret);
    }
    {
        VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::OCL);
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{lzeroTestingVariableName, "Default Level Zero"},
                                                                     {oclTestingVariableName, setString.c_str()}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::none;

        retString = environmentVariableReader->getSetting("TestingVariable", defaultString, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::neoOcl, type);
        EXPECT_EQ(0, retString.compare(setString));
    }
    {
        VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{lzeroTestingVariableName, "Default Level Zero"},
                                                                     {oclTestingVariableName, setString.c_str()}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::none;

        retString = environmentVariableReader->getSetting("TestingVariable", defaultString, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::neoL0, type);
        EXPECT_EQ(0, retString.compare("Default Level Zero"));
    }
}

TEST_F(DebugEnvReaderTests, givenMaxInt64AsEnvWhenGetSettingThenProperValueIsReturned) {
    const char *testingVariableName = "TestingVariable";
    const char *testingVariableValue = "9223372036854775807";
    int64_t expectedValue = 9223372036854775807;
    int64_t defaultValue = 0;

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{testingVariableName, testingVariableValue}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    EXPECT_EQ(expectedValue, environmentVariableReader->getSetting(testingVariableName, defaultValue));
    EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
}

TEST_F(DebugEnvReaderTests, GivenUnsetVariableThenDefaultValueIsReturned) {
    int32_t ret;
    std::string retString;
    std::string defaultString = "Default Value";

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    ret = environmentVariableReader->getSetting("TestingVariable", 1);
    EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
    EXPECT_EQ(1, ret);

    retString = environmentVariableReader->getSetting("TestingVariable", defaultString);
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

        ret = environmentVariableReader->getSetting("TestingVariable", defaultValue);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(expectedValue, ret);
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

        ret = environmentVariableReader->getSetting("TestingVariable", defaultValue);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(defaultValue, ret);
    }
}

TEST_F(DebugEnvReaderTests, givenEnvironmentVariableReaderWhenCreateOsReaderWithStringThenNotNullPointer) {
    std::unique_ptr<SettingsReader> settingsReader(SettingsReader::createOsReader(false, ""));
    EXPECT_NE(nullptr, settingsReader);
}

TEST_F(DebugEnvReaderTests, givenTooLongEnvValueWhenGetEnvironmentVariableIsCalledThenNullptrIsReturned) {
    {
        auto maxAllowedEnvVariableSize = CommonConstants::maxAllowedEnvVariableSize;
        const char *testingVariableName = "test";
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        {

            std::string veryLongPath(maxAllowedEnvVariableSize + 1u, 'a');
            veryLongPath.back() = '\0';
            std::unordered_map<std::string, std::string> mockableEnvs = {{testingVariableName, veryLongPath.c_str()}};
            VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
            auto envValue = IoFunctions::getEnvironmentVariable(testingVariableName);

            ASSERT_NE(nullptr, veryLongPath.c_str());
            EXPECT_EQ(nullptr, envValue);
        }

        {
            std::string goodPath(maxAllowedEnvVariableSize, 'a');
            goodPath.back() = '\0';
            std::unordered_map<std::string, std::string> mockableEnvs = {{testingVariableName, goodPath.c_str()}};
            VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
            auto envValue = IoFunctions::getEnvironmentVariable(testingVariableName);

            ASSERT_NE(nullptr, goodPath.c_str());
            EXPECT_EQ(mockableEnvs.at(testingVariableName).c_str(), envValue);
        }
    }
}

} // namespace NEO
