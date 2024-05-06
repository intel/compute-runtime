/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/windows/registry_reader_tests.h"

#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {

enum class DebugVarPrefix : uint8_t;

using RegistryReaderTest = ::testing::Test;

namespace SysCalls {
extern uint32_t regOpenKeySuccessCount;
extern uint32_t regQueryValueSuccessCount;
extern uint64_t regQueryValueExpectedData;
} // namespace SysCalls

TEST_F(RegistryReaderTest, givenRegistryReaderWhenItIsCreatedWithUserScopeSetToFalseThenItsHkeyTypeIsInitializedToHkeyLocalMachine) {
    bool userScope = false;
    TestedRegistryReader registryReader(userScope);
    EXPECT_EQ(HKEY_LOCAL_MACHINE, registryReader.getHkeyType());
}

TEST_F(RegistryReaderTest, givenRegistryReaderWhenItIsCreatedWithUserScopeSetToTrueThenItsHkeyTypeIsInitializedHkeyCurrentUser) {
    bool userScope = true;
    TestedRegistryReader registryReader(userScope);
    EXPECT_EQ(HKEY_CURRENT_USER, registryReader.getHkeyType());
}

TEST_F(RegistryReaderTest, givenRegistryReaderWhenCallAppSpecificLocationThenReturnCurrentProcessName) {
    char buff[MAX_PATH];
    GetModuleFileNameA(nullptr, buff, MAX_PATH);

    TestedRegistryReader registryReader(false);
    const char *ret = registryReader.appSpecificLocation("cl_cache_dir");
    EXPECT_STREQ(buff, ret);
}

TEST_F(RegistryReaderTest, givenRegistryReaderWhenRegKeyNotExistThenReturnDefaultValue) {
    std::string regKey = "notExistPath";
    std::string value = "defaultValue";
    TestedRegistryReader registryReader(regKey);

    EXPECT_EQ(value, registryReader.getSetting("", value));
}

TEST_F(RegistryReaderTest, givenRegistryReaderWhenRegKeyNotExistThenReturnDefaultValuePrefix) {
    std::string regKey = "notExistPath";
    std::string value = "defaultValue";
    TestedRegistryReader registryReader(regKey);

    DebugVarPrefix type = DebugVarPrefix::none;
    EXPECT_EQ(value, registryReader.getSetting("", value, type));
}

TEST_F(RegistryReaderTest, givenRegistryReaderWhenItIsCreatedWithRegKeySpecifiedThenRegKeyIsInitializedAccordingly) {
    std::string regKey = "Software\\Intel\\IGFX\\OCL\\regKey";
    TestedRegistryReader registryReader(regKey);
    EXPECT_STREQ(regKey.c_str(), registryReader.getRegKey());
}

TEST_F(RegistryReaderTest, givenRegistryReaderWhenEnvironmentVariableExistsThenReturnCorrectValue) {
    const char *envVar = "TestedEnvironmentVariable";
    std::string value = "defaultValue";
    TestedRegistryReader registryReader("");
    EXPECT_EQ("TestedEnvironmentVariableValue", registryReader.getSetting(envVar, value));
}

TEST_F(RegistryReaderTest, givenRegistryReaderWhenEnvironmentVariableExistsThenReturnCorrectValuePrefix) {
    const char *envVar = "TestedEnvironmentVariable";
    std::string value = "defaultValue";
    TestedRegistryReader registryReader("");
    DebugVarPrefix type = DebugVarPrefix::none;
    EXPECT_EQ("TestedEnvironmentVariableValue", registryReader.getSetting(envVar, value, type));
    EXPECT_EQ(DebugVarPrefix::none, type);
}

TEST_F(RegistryReaderTest, givenRegistryReaderWhenPrefixedEnvironmentVariableExistsThenReturnCorrectValue) {
    const char *envVar = "TestedEnvironmentVariableWithPrefix";
    std::string value = "defaultValue";
    TestedRegistryReader registryReader("");
    DebugVarPrefix type = DebugVarPrefix::none;
    EXPECT_EQ("TestedEnvironmentVariableValueWithPrefix", registryReader.getSetting(envVar, value, type));
    EXPECT_EQ(DebugVarPrefix::neo, type);
}

TEST_F(RegistryReaderTest, givenRegistryReaderWhenEnvironmentIntVariableExistsThenReturnCorrectValue) {
    const char *envVar = "TestedEnvironmentIntVariable";
    int32_t value = -1;
    TestedRegistryReader registryReader("");
    EXPECT_EQ(1234, registryReader.getSetting(envVar, value));
}

TEST_F(RegistryReaderTest, givenRegistryReaderWhenEnvironmentIntVariableExistsThenReturnCorrectValuePrefix) {
    const char *envVar = "TestedEnvironmentIntVariable";
    int32_t value = -1;
    TestedRegistryReader registryReader("");
    DebugVarPrefix type = DebugVarPrefix::none;
    EXPECT_EQ(1234, registryReader.getSetting(envVar, value, type));
    EXPECT_EQ(DebugVarPrefix::none, type);
}

TEST_F(RegistryReaderTest, givenRegistryReaderWhenPrefixedEnvironmentIntVariableExistsThenReturnCorrectValue) {
    const char *envVar = "TestedEnvironmentIntVariableWithPrefix";
    int32_t value = -1;
    TestedRegistryReader registryReader("");
    DebugVarPrefix type = DebugVarPrefix::none;
    EXPECT_EQ(5678, registryReader.getSetting(envVar, value, type));
    EXPECT_EQ(DebugVarPrefix::neo, type);
}

TEST_F(RegistryReaderTest, givenRegistryReaderWhenEnvironmentInt64VariableExistsThenReturnCorrectValue) {
    const char *envVar = "TestedEnvironmentInt64Variable";
    int64_t expectedValue = 9223372036854775807;
    int64_t defaultValue = 0;
    TestedRegistryReader registryReader("");
    EXPECT_EQ(expectedValue, registryReader.getSetting(envVar, defaultValue));
}

TEST_F(RegistryReaderTest, givenRegistryReaderWhenEnvironmentInt64VariableExistsThenReturnCorrectValuePrefix) {
    const char *envVar = "TestedEnvironmentInt64Variable";
    int64_t expectedValue = 9223372036854775807;
    int64_t defaultValue = 0;
    TestedRegistryReader registryReader("");
    DebugVarPrefix type = DebugVarPrefix::none;
    EXPECT_EQ(expectedValue, registryReader.getSetting(envVar, defaultValue, type));
    EXPECT_EQ(DebugVarPrefix::none, type);
}

TEST_F(RegistryReaderTest, givenRegistryReaderWhenPrefixedEnvironmentInt64VariableExistsThenReturnCorrectValue) {
    const char *envVar = "TestedEnvironmentInt64VariableWithPrefix";
    int64_t expectedValue = 9223372036854775806;
    int64_t defaultValue = 0;
    TestedRegistryReader registryReader("");
    DebugVarPrefix type = DebugVarPrefix::none;
    EXPECT_EQ(expectedValue, registryReader.getSetting(envVar, defaultValue, type));
    EXPECT_EQ(DebugVarPrefix::neo, type);
}

struct DebugReaderWithRegistryAndEnvTest : ::testing::Test {
    VariableBackup<uint32_t> openRegCountBackup{&SysCalls::regOpenKeySuccessCount};
    VariableBackup<uint32_t> queryRegCountBackup{&SysCalls::regQueryValueSuccessCount};
    TestedRegistryReader registryReader{std::string("")};
};

TEST_F(DebugReaderWithRegistryAndEnvTest, givenIntDebugKeyWhenReadFromRegistrySucceedsThenReturnObtainedValue) {
    SysCalls::regOpenKeySuccessCount = 1u;
    SysCalls::regQueryValueSuccessCount = 1u;

    EXPECT_EQ(1, registryReader.getSetting("settingSourceInt", 0));
}

TEST_F(DebugReaderWithRegistryAndEnvTest, givenIntDebugKeyWhenReadFromRegistrySucceedsThenReturnObtainedValuePrefix) {
    SysCalls::regOpenKeySuccessCount = 1u;
    SysCalls::regQueryValueSuccessCount = 1u;

    DebugVarPrefix type = DebugVarPrefix::none;
    EXPECT_EQ(1, registryReader.getSetting("settingSourceInt", 0, type));
    EXPECT_EQ(DebugVarPrefix::none, type);
}

TEST_F(DebugReaderWithRegistryAndEnvTest, givenInt64DebugKeyWhenReadFromRegistrySucceedsThenReturnObtainedValue) {
    SysCalls::regOpenKeySuccessCount = 1u;
    SysCalls::regQueryValueSuccessCount = 1u;

    int expectedValue = 0xeeeeeeee;
    EXPECT_EQ(expectedValue, registryReader.getSetting("settingSourceInt64", 0));
}

TEST_F(DebugReaderWithRegistryAndEnvTest, givenInt64DebugKeyWhenReadFromRegistrySucceedsThenReturnObtainedValuePrefix) {
    SysCalls::regOpenKeySuccessCount = 1u;
    SysCalls::regQueryValueSuccessCount = 1u;

    DebugVarPrefix type = DebugVarPrefix::none;
    int expectedValue = 0xeeeeeeee;
    EXPECT_EQ(expectedValue, registryReader.getSetting("settingSourceInt64", 0, type));
    EXPECT_EQ(DebugVarPrefix::none, type);
}

TEST_F(DebugReaderWithRegistryAndEnvTest, givenIntDebugKeyWhenQueryValueFailsThenObtainValueFromEnv) {
    SysCalls::regOpenKeySuccessCount = 1u;
    SysCalls::regQueryValueSuccessCount = 0u;

    EXPECT_EQ(2, registryReader.getSetting("settingSourceInt", 0));
}

TEST_F(DebugReaderWithRegistryAndEnvTest, givenIntDebugKeyWhenQueryValueFailsThenObtainValueFromEnvPrefix) {
    SysCalls::regOpenKeySuccessCount = 1u;
    SysCalls::regQueryValueSuccessCount = 0u;

    DebugVarPrefix type = DebugVarPrefix::none;
    EXPECT_EQ(2, registryReader.getSetting("settingSourceInt", 0, type));
    EXPECT_EQ(DebugVarPrefix::none, type);
}

TEST_F(DebugReaderWithRegistryAndEnvTest, givenIntDebugKeyWhenOpenKeyFailsThenObtainValueFromEnv) {
    SysCalls::regOpenKeySuccessCount = 0u;
    SysCalls::regQueryValueSuccessCount = 0u;

    EXPECT_EQ(2, registryReader.getSetting("settingSourceInt", 0));
}

TEST_F(DebugReaderWithRegistryAndEnvTest, givenIntDebugKeyWhenOpenKeyFailsThenObtainValueFromEnvPrefix) {
    SysCalls::regOpenKeySuccessCount = 0u;
    SysCalls::regQueryValueSuccessCount = 0u;

    DebugVarPrefix type = DebugVarPrefix::none;
    EXPECT_EQ(2, registryReader.getSetting("settingSourceInt", 0, type));
    EXPECT_EQ(DebugVarPrefix::none, type);
}

TEST_F(DebugReaderWithRegistryAndEnvTest, givenStringDebugKeyWhenReadFromRegistrySucceedsThenReturnObtainedValue) {
    std::string defaultValue("default");
    SysCalls::regOpenKeySuccessCount = 1u;
    SysCalls::regQueryValueSuccessCount = 2u;

    EXPECT_STREQ("registry", registryReader.getSetting("settingSourceString", defaultValue).c_str());
}

TEST_F(DebugReaderWithRegistryAndEnvTest, givenStringDebugKeyWhenReadFromRegistrySucceedsThenReturnObtainedValuePrefix) {
    std::string defaultValue("default");
    SysCalls::regOpenKeySuccessCount = 1u;
    SysCalls::regQueryValueSuccessCount = 2u;

    DebugVarPrefix type = DebugVarPrefix::none;
    EXPECT_STREQ("registry", registryReader.getSetting("settingSourceString", defaultValue, type).c_str());
    EXPECT_EQ(DebugVarPrefix::none, type);
}

TEST_F(DebugReaderWithRegistryAndEnvTest, givenStringDebugKeyWhenQueryValueFailsThenObtainValueFromEnv) {
    std::string defaultValue("default");
    SysCalls::regOpenKeySuccessCount = 1u;
    SysCalls::regQueryValueSuccessCount = 0u;

    EXPECT_STREQ("environment", registryReader.getSetting("settingSourceString", defaultValue).c_str());

    SysCalls::regOpenKeySuccessCount = 1u;
    SysCalls::regQueryValueSuccessCount = 1u;

    EXPECT_STREQ("environment", registryReader.getSetting("settingSourceString", defaultValue).c_str());
}

TEST_F(DebugReaderWithRegistryAndEnvTest, givenStringDebugKeyWhenQueryValueFailsThenObtainValueFromEnvPrefix) {
    std::string defaultValue("default");
    SysCalls::regOpenKeySuccessCount = 1u;
    SysCalls::regQueryValueSuccessCount = 0u;

    DebugVarPrefix type = DebugVarPrefix::none;
    EXPECT_STREQ("environment", registryReader.getSetting("settingSourceString", defaultValue, type).c_str());
    EXPECT_EQ(DebugVarPrefix::none, type);

    SysCalls::regOpenKeySuccessCount = 1u;
    SysCalls::regQueryValueSuccessCount = 1u;

    EXPECT_STREQ("environment", registryReader.getSetting("settingSourceString", defaultValue, type).c_str());
    EXPECT_EQ(DebugVarPrefix::none, type);
}

TEST_F(DebugReaderWithRegistryAndEnvTest, givenStringDebugKeyWhenOpenKeyFailsThenObtainValueFromEnv) {
    std::string defaultValue("default");
    SysCalls::regOpenKeySuccessCount = 0u;
    SysCalls::regQueryValueSuccessCount = 0u;

    EXPECT_STREQ("environment", registryReader.getSetting("settingSourceString", defaultValue).c_str());
}

TEST_F(DebugReaderWithRegistryAndEnvTest, givenStringDebugKeyWhenOpenKeyFailsThenObtainValueFromEnvPrefix) {
    std::string defaultValue("default");
    SysCalls::regOpenKeySuccessCount = 0u;
    SysCalls::regQueryValueSuccessCount = 0u;

    DebugVarPrefix type = DebugVarPrefix::none;
    EXPECT_STREQ("environment", registryReader.getSetting("settingSourceString", defaultValue, type).c_str());
    EXPECT_EQ(DebugVarPrefix::none, type);
}

TEST_F(DebugReaderWithRegistryAndEnvTest, givenBinaryDebugKeyWhenReadFromRegistrySucceedsThenReturnObtainedValue) {
    std::string defaultValue("default");
    SysCalls::regOpenKeySuccessCount = 1u;
    SysCalls::regQueryValueSuccessCount = 2u;

    EXPECT_STREQ("registry", registryReader.getSetting("settingSourceBinary", defaultValue).c_str());
}

TEST_F(DebugReaderWithRegistryAndEnvTest, givenBinaryDebugKeyWhenReadFromRegistrySucceedsThenReturnObtainedValuePrefix) {
    std::string defaultValue("default");
    SysCalls::regOpenKeySuccessCount = 1u;
    SysCalls::regQueryValueSuccessCount = 2u;

    DebugVarPrefix type = DebugVarPrefix::none;
    EXPECT_STREQ("registry", registryReader.getSetting("settingSourceBinary", defaultValue, type).c_str());
    EXPECT_EQ(DebugVarPrefix::none, type);
}

TEST_F(DebugReaderWithRegistryAndEnvTest, givenBinaryDebugKeyOnlyInRegistryWhenReadFromRegistryFailsThenReturnDefaultValue) {
    std::string defaultValue("default");
    SysCalls::regOpenKeySuccessCount = 1u;
    SysCalls::regQueryValueSuccessCount = 1u;

    EXPECT_STREQ("default", registryReader.getSetting("settingSourceBinary", defaultValue).c_str());

    SysCalls::regOpenKeySuccessCount = 1u;
    SysCalls::regQueryValueSuccessCount = 0u;

    EXPECT_STREQ("default", registryReader.getSetting("settingSourceBinary", defaultValue).c_str());

    SysCalls::regOpenKeySuccessCount = 0u;
    SysCalls::regQueryValueSuccessCount = 0u;

    EXPECT_STREQ("default", registryReader.getSetting("settingSourceBinary", defaultValue).c_str());
}

TEST_F(DebugReaderWithRegistryAndEnvTest, givenBinaryDebugKeyOnlyInRegistryWhenReadFromRegistryFailsThenReturnDefaultValuePrefix) {
    std::string defaultValue("default");
    SysCalls::regOpenKeySuccessCount = 1u;
    SysCalls::regQueryValueSuccessCount = 1u;

    DebugVarPrefix type = DebugVarPrefix::none;
    EXPECT_STREQ("default", registryReader.getSetting("settingSourceBinary", defaultValue, type).c_str());
    EXPECT_EQ(DebugVarPrefix::none, type);

    SysCalls::regOpenKeySuccessCount = 1u;
    SysCalls::regQueryValueSuccessCount = 0u;

    EXPECT_STREQ("default", registryReader.getSetting("settingSourceBinary", defaultValue, type).c_str());
    EXPECT_EQ(DebugVarPrefix::none, type);

    SysCalls::regOpenKeySuccessCount = 0u;
    SysCalls::regQueryValueSuccessCount = 0u;

    EXPECT_STREQ("default", registryReader.getSetting("settingSourceBinary", defaultValue, type).c_str());
    EXPECT_EQ(DebugVarPrefix::none, type);
}

TEST_F(RegistryReaderTest, givenRegistryKeyPresentWhenValueIsZeroThenExpectBooleanFalse) {
    std::string regKey = "notExistPath";
    std::string keyName = "boolRegistryKey";

    bool defaultValue = false;
    SysCalls::regOpenKeySuccessCount = 1;
    SysCalls::regQueryValueSuccessCount = 1;
    SysCalls::regQueryValueExpectedData = 0ull;

    TestedRegistryReader registryReader(regKey);
    bool value = registryReader.getSetting(keyName.c_str(), defaultValue);
    EXPECT_FALSE(value);
}

TEST_F(RegistryReaderTest, givenRegistryKeyPresentWhenValueIsZeroThenExpectBooleanFalsePrefix) {
    std::string regKey = "notExistPath";
    std::string keyName = "boolRegistryKey";

    bool defaultValue = false;
    SysCalls::regOpenKeySuccessCount = 1;
    SysCalls::regQueryValueSuccessCount = 1;
    SysCalls::regQueryValueExpectedData = 0ull;

    TestedRegistryReader registryReader(regKey);
    DebugVarPrefix type = DebugVarPrefix::none;
    bool value = registryReader.getSetting(keyName.c_str(), defaultValue, type);
    EXPECT_FALSE(value);
    EXPECT_EQ(DebugVarPrefix::none, type);
}

TEST_F(RegistryReaderTest, givenRegistryKeyNotPresentWhenDefaulValueIsFalseOrTrueThenExpectReturnIsMatchingFalseOrTrue) {
    std::string regKey = "notExistPath";
    std::string keyName = "boolRegistryKey";

    bool defaultValue = false;
    SysCalls::regOpenKeySuccessCount = 1;
    SysCalls::regQueryValueSuccessCount = 0;
    SysCalls::regQueryValueExpectedData = 1ull;

    TestedRegistryReader registryReader(regKey);
    bool value = registryReader.getSetting(keyName.c_str(), defaultValue);
    EXPECT_FALSE(value);

    defaultValue = true;
    SysCalls::regOpenKeySuccessCount = 1;
    SysCalls::regQueryValueSuccessCount = 0;
    SysCalls::regQueryValueExpectedData = 0ull;

    value = registryReader.getSetting(keyName.c_str(), defaultValue);
    EXPECT_TRUE(value);
}

TEST_F(RegistryReaderTest, givenRegistryKeyNotPresentWhenDefaulValueIsFalseOrTrueThenExpectReturnIsMatchingFalseOrTruePrefix) {
    std::string regKey = "notExistPath";
    std::string keyName = "boolRegistryKey";

    bool defaultValue = false;
    SysCalls::regOpenKeySuccessCount = 1;
    SysCalls::regQueryValueSuccessCount = 0;
    SysCalls::regQueryValueExpectedData = 1ull;

    TestedRegistryReader registryReader(regKey);
    DebugVarPrefix type = DebugVarPrefix::none;
    bool value = registryReader.getSetting(keyName.c_str(), defaultValue, type);
    EXPECT_FALSE(value);
    EXPECT_EQ(DebugVarPrefix::none, type);

    defaultValue = true;
    SysCalls::regOpenKeySuccessCount = 1;
    SysCalls::regQueryValueSuccessCount = 0;
    SysCalls::regQueryValueExpectedData = 0ull;

    value = registryReader.getSetting(keyName.c_str(), defaultValue, type);
    EXPECT_TRUE(value);
    EXPECT_EQ(DebugVarPrefix::none, type);
}

TEST_F(RegistryReaderTest, givenRegistryKeyPresentWhenValueIsNonZeroInHigherDwordThenExpectBooleanFalse) {
    std::string regKey = "notExistPath";
    std::string keyName = "boolRegistryKey";

    bool defaultValue = true;
    SysCalls::regOpenKeySuccessCount = 1;
    SysCalls::regQueryValueSuccessCount = 1;
    SysCalls::regQueryValueExpectedData = 1ull << 32;

    TestedRegistryReader registryReader(regKey);
    bool value = registryReader.getSetting(keyName.c_str(), defaultValue);
    EXPECT_FALSE(value);
}

TEST_F(RegistryReaderTest, givenRegistryKeyPresentWhenValueIsNonZeroInHigherDwordThenExpectBooleanFalsePrefix) {
    std::string regKey = "notExistPath";
    std::string keyName = "boolRegistryKey";

    bool defaultValue = true;
    SysCalls::regOpenKeySuccessCount = 1;
    SysCalls::regQueryValueSuccessCount = 1;
    SysCalls::regQueryValueExpectedData = 1ull << 32;

    TestedRegistryReader registryReader(regKey);
    DebugVarPrefix type = DebugVarPrefix::none;
    bool value = registryReader.getSetting(keyName.c_str(), defaultValue, type);
    EXPECT_FALSE(value);
    EXPECT_EQ(DebugVarPrefix::none, type);
}

TEST_F(RegistryReaderTest, givenRegistryKeyPresentWhenValueIsNonZeroInLowerDwordThenExpectBooleanTrue) {
    std::string regKey = "notExistPath";
    std::string keyName = "boolRegistryKey";

    bool defaultValue = false;
    SysCalls::regOpenKeySuccessCount = 1;
    SysCalls::regQueryValueSuccessCount = 1;
    SysCalls::regQueryValueExpectedData = 1ull;

    TestedRegistryReader registryReader(regKey);
    bool value = registryReader.getSetting(keyName.c_str(), defaultValue);
    EXPECT_TRUE(value);
}

TEST_F(RegistryReaderTest, givenRegistryKeyPresentWhenValueIsNonZeroInLowerDwordThenExpectBooleanTruePrefix) {
    std::string regKey = "notExistPath";
    std::string keyName = "boolRegistryKey";

    bool defaultValue = false;
    SysCalls::regOpenKeySuccessCount = 1;
    SysCalls::regQueryValueSuccessCount = 1;
    SysCalls::regQueryValueExpectedData = 1ull;

    TestedRegistryReader registryReader(regKey);
    DebugVarPrefix type = DebugVarPrefix::none;
    bool value = registryReader.getSetting(keyName.c_str(), defaultValue, type);
    EXPECT_TRUE(value);
    EXPECT_EQ(DebugVarPrefix::none, type);
}

TEST_F(RegistryReaderTest, givenRegistryKeyPresentWhenValueIsNonZeroInBothDwordsThenExpectBooleanTrue) {
    std::string regKey = "notExistPath";
    std::string keyName = "boolRegistryKey";

    bool defaultValue = false;
    SysCalls::regOpenKeySuccessCount = 1;
    SysCalls::regQueryValueSuccessCount = 1;
    SysCalls::regQueryValueExpectedData = 1ull | (1ull << 32);

    TestedRegistryReader registryReader(regKey);
    bool value = registryReader.getSetting(keyName.c_str(), defaultValue);
    EXPECT_TRUE(value);
}

TEST_F(RegistryReaderTest, givenRegistryKeyPresentWhenValueIsNonZeroInBothDwordsThenExpectBooleanTruePrefix) {
    std::string regKey = "notExistPath";
    std::string keyName = "boolRegistryKey";

    bool defaultValue = false;
    SysCalls::regOpenKeySuccessCount = 1;
    SysCalls::regQueryValueSuccessCount = 1;
    SysCalls::regQueryValueExpectedData = 1ull | (1ull << 32);

    TestedRegistryReader registryReader(regKey);

    DebugVarPrefix type = DebugVarPrefix::none;
    bool value = registryReader.getSetting(keyName.c_str(), defaultValue, type);
    EXPECT_TRUE(value);
    EXPECT_EQ(DebugVarPrefix::none, type);
}
} // namespace NEO
