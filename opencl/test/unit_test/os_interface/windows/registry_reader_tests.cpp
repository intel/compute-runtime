/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/os_interface/windows/registry_reader_tests.h"

#include "opencl/test/unit_test/helpers/variable_backup.h"
#include "test.h"

using namespace NEO;

using RegistryReaderTest = ::testing::Test;

extern uint32_t regOpenKeySuccessCount;
extern uint32_t regQueryValueSuccessCount;

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

TEST_F(RegistryReaderTest, givenRegistryReaderWhenItIsCreatedWithRegKeySpecifiedThenRegKeyIsInitializedAccordingly) {
    std::string regKey = "Software\\Intel\\IGFX\\OCL\\regKey";
    TestedRegistryReader registryReader(regKey);
    EXPECT_STREQ(regKey.c_str(), registryReader.getRegKey());
}

TEST_F(RegistryReaderTest, givenRegistryReaderWhenEnvironmentVariableExistsThenReturnCorrectValue) {
    char *envVar = "TestedEnvironmentVariable";
    std::string value = "defaultValue";
    TestedRegistryReader registryReader("");
    EXPECT_EQ("TestedEnvironmentVariableValue", registryReader.getSetting(envVar, value));
}

TEST_F(RegistryReaderTest, givenRegistryReaderWhenEnvironmentIntVariableExistsThenReturnCorrectValue) {
    char *envVar = "TestedEnvironmentIntVariable";
    int32_t value = -1;
    TestedRegistryReader registryReader("");
    EXPECT_EQ(1234, registryReader.getSetting(envVar, value));
}

struct DebugReaderWithRegistryAndEnvTest : ::testing::Test {
    VariableBackup<uint32_t> openRegCountBackup{&regOpenKeySuccessCount};
    VariableBackup<uint32_t> queryRegCountBackup{&regQueryValueSuccessCount};
    TestedRegistryReader registryReader{""};
};

TEST_F(DebugReaderWithRegistryAndEnvTest, givenIntDebugKeyWhenReadFromRegistrySucceedsThenReturnObtainedValue) {
    regOpenKeySuccessCount = 1u;
    regQueryValueSuccessCount = 1u;

    EXPECT_EQ(1, registryReader.getSetting("settingSourceInt", 0));
}

TEST_F(DebugReaderWithRegistryAndEnvTest, givenInt64DebugKeyWhenReadFromRegistrySucceedsThenReturnObtainedValue) {
    regOpenKeySuccessCount = 1u;
    regQueryValueSuccessCount = 1u;

    EXPECT_EQ(0xffffffffeeeeeeee, registryReader.getSetting("settingSourceInt64", 0));
}

TEST_F(DebugReaderWithRegistryAndEnvTest, givenIntDebugKeyWhenQueryValueFailsThenObtainValueFromEnv) {
    regOpenKeySuccessCount = 1u;
    regQueryValueSuccessCount = 0u;

    EXPECT_EQ(2, registryReader.getSetting("settingSourceInt", 0));
}

TEST_F(DebugReaderWithRegistryAndEnvTest, givenIntDebugKeyWhenOpenKeyFailsThenObtainValueFromEnv) {
    regOpenKeySuccessCount = 0u;
    regQueryValueSuccessCount = 0u;

    EXPECT_EQ(2, registryReader.getSetting("settingSourceInt", 0));
}

TEST_F(DebugReaderWithRegistryAndEnvTest, givenStringDebugKeyWhenReadFromRegistrySucceedsThenReturnObtainedValue) {
    std::string defaultValue("default");
    regOpenKeySuccessCount = 1u;
    regQueryValueSuccessCount = 2u;

    EXPECT_STREQ("registry", registryReader.getSetting("settingSourceString", defaultValue).c_str());
}

TEST_F(DebugReaderWithRegistryAndEnvTest, givenStringDebugKeyWhenQueryValueFailsThenObtainValueFromEnv) {
    std::string defaultValue("default");
    regOpenKeySuccessCount = 1u;
    regQueryValueSuccessCount = 0u;

    EXPECT_STREQ("environment", registryReader.getSetting("settingSourceString", defaultValue).c_str());

    regOpenKeySuccessCount = 1u;
    regQueryValueSuccessCount = 1u;

    EXPECT_STREQ("environment", registryReader.getSetting("settingSourceString", defaultValue).c_str());
}

TEST_F(DebugReaderWithRegistryAndEnvTest, givenStringDebugKeyWhenOpenKeyFailsThenObtainValueFromEnv) {
    std::string defaultValue("default");
    regOpenKeySuccessCount = 0u;
    regQueryValueSuccessCount = 0u;

    EXPECT_STREQ("environment", registryReader.getSetting("settingSourceString", defaultValue).c_str());
}

TEST_F(DebugReaderWithRegistryAndEnvTest, givenBinaryDebugKeyWhenReadFromRegistrySucceedsThenReturnObtainedValue) {
    std::string defaultValue("default");
    regOpenKeySuccessCount = 1u;
    regQueryValueSuccessCount = 2u;

    EXPECT_STREQ("registry", registryReader.getSetting("settingSourceBinary", defaultValue).c_str());
}
TEST_F(DebugReaderWithRegistryAndEnvTest, givenBinaryDebugKeyOnlyInRegistryWhenReadFromRegistryFailsThenReturnDefaultValue) {
    std::string defaultValue("default");
    regOpenKeySuccessCount = 1u;
    regQueryValueSuccessCount = 1u;

    EXPECT_STREQ("default", registryReader.getSetting("settingSourceBinary", defaultValue).c_str());

    regOpenKeySuccessCount = 1u;
    regQueryValueSuccessCount = 0u;

    EXPECT_STREQ("default", registryReader.getSetting("settingSourceBinary", defaultValue).c_str());

    regOpenKeySuccessCount = 0u;
    regQueryValueSuccessCount = 0u;

    EXPECT_STREQ("default", registryReader.getSetting("settingSourceBinary", defaultValue).c_str());
}