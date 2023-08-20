/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/debug_env_reader.h"

#include "shared/source/helpers/api_specific_config.h"
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
        DebugVarPrefix type = DebugVarPrefix::None;

        ret = evr->getSetting(testingVariableName, 1, type);
        EXPECT_EQ(3u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::None, type);
        EXPECT_EQ(1234, ret);
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{testingVariableName, setString.c_str()}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::None;

        retString = evr->getSetting("TestingVariable", defaultString, type);
        EXPECT_EQ(3u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::None, type);
        EXPECT_EQ(0, retString.compare(setString));
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{testingVariableName, "0"}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::None;

        ret = evr->getSetting(testingVariableName, defaultBoolValue, type);
        EXPECT_EQ(3u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::None, type);
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
        DebugVarPrefix type = DebugVarPrefix::None;

        ret = evr->getSetting(testingVariableName, 1, type);
        EXPECT_EQ(2u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::Neo, type);
        EXPECT_EQ(1234, ret);
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{neoTestingVariableName, setString.c_str()}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::None;

        retString = evr->getSetting("TestingVariable", defaultString, type);
        EXPECT_EQ(2u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::Neo, type);
        EXPECT_EQ(0, retString.compare(setString));
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{neoTestingVariableName, "0"}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::None;

        ret = evr->getSetting(testingVariableName, defaultBoolValue, type);
        EXPECT_EQ(2u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::Neo, type);
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
        DebugVarPrefix type = DebugVarPrefix::None;

        ret = evr->getSetting(testingVariableName, 1, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::Neo_Ocl, type);
        EXPECT_EQ(1234, ret);
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{oclTestingVariableName, setString.c_str()}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::None;

        retString = evr->getSetting("TestingVariable", defaultString, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::Neo_Ocl, type);
        EXPECT_EQ(0, retString.compare(setString));
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{oclTestingVariableName, "0"}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::None;

        ret = evr->getSetting(testingVariableName, defaultBoolValue, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::Neo_Ocl, type);
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
        DebugVarPrefix type = DebugVarPrefix::None;
        ret = evr->getSetting(testingVariableName, 1, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::Neo_L0, type);
        EXPECT_EQ(1234, ret);
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{lzeroTestingVariableName, setString.c_str()}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::None;

        retString = evr->getSetting("TestingVariable", defaultString, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::Neo_L0, type);
        EXPECT_EQ(0, retString.compare(setString));
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{lzeroTestingVariableName, "0"}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::None;

        ret = evr->getSetting(testingVariableName, defaultBoolValue, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::Neo_L0, type);
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
        DebugVarPrefix type = DebugVarPrefix::None;
        ret = evr->getSetting(testingVariableName, 1, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::Neo_L0, type);
        EXPECT_EQ(1234, ret);
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{testingVariableName, "1"},
                                                                     {neoTestingVariableName, "2"}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::None;
        ret = evr->getSetting(testingVariableName, 1, type);
        EXPECT_EQ(2u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::Neo, type);
        EXPECT_EQ(2, ret);
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{testingVariableName, "Default None"},
                                                                     {neoTestingVariableName, "Default Neo"},
                                                                     {lzeroTestingVariableName, setString.c_str()}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::None;

        retString = evr->getSetting("TestingVariable", defaultString, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::Neo_L0, type);
        EXPECT_EQ(0, retString.compare(setString));
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{testingVariableName, "Default None"},
                                                                     {neoTestingVariableName, "Default Neo"}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::None;

        retString = evr->getSetting("TestingVariable", defaultString, type);
        EXPECT_EQ(2u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::Neo, type);
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
        DebugVarPrefix type = DebugVarPrefix::None;
        ret = evr->getSetting(testingVariableName, 1, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::Neo_Ocl, type);
        EXPECT_EQ(1234, ret);
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{testingVariableName, "1"},
                                                                     {neoTestingVariableName, "2"}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::None;
        ret = evr->getSetting(testingVariableName, 1, type);
        EXPECT_EQ(2u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::Neo, type);
        EXPECT_EQ(2, ret);
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{testingVariableName, "Default None"},
                                                                     {neoTestingVariableName, "Default Neo"},
                                                                     {oclTestingVariableName, setString.c_str()}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::None;

        retString = evr->getSetting("TestingVariable", defaultString, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::Neo_Ocl, type);
        EXPECT_EQ(0, retString.compare(setString));
    }
    {
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{testingVariableName, "Default None"},
                                                                     {neoTestingVariableName, "Default Neo"}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::None;

        retString = evr->getSetting("TestingVariable", defaultString, type);
        EXPECT_EQ(2u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::Neo, type);
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
        DebugVarPrefix type = DebugVarPrefix::None;
        ret = evr->getSetting(testingVariableName, 1, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::Neo_Ocl, type);
        EXPECT_EQ(1234, ret);
    }
    {
        VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{lzeroTestingVariableName, "5678"},
                                                                     {oclTestingVariableName, testingVariableValue}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::None;
        ret = evr->getSetting(testingVariableName, 1, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::Neo_L0, type);
        EXPECT_EQ(5678, ret);
    }
    {
        VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::OCL);
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{lzeroTestingVariableName, "Default Level Zero"},
                                                                     {oclTestingVariableName, setString.c_str()}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::None;

        retString = evr->getSetting("TestingVariable", defaultString, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::Neo_Ocl, type);
        EXPECT_EQ(0, retString.compare(setString));
    }
    {
        VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
        VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
        std::unordered_map<std::string, std::string> mockableEnvs = {{lzeroTestingVariableName, "Default Level Zero"},
                                                                     {oclTestingVariableName, setString.c_str()}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
        DebugVarPrefix type = DebugVarPrefix::None;

        retString = evr->getSetting("TestingVariable", defaultString, type);
        EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
        EXPECT_EQ(DebugVarPrefix::Neo_L0, type);
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

    EXPECT_EQ(expectedValue, evr->getSetting(testingVariableName, defaultValue));
    EXPECT_EQ(1u, IoFunctions::mockGetenvCalled);
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
