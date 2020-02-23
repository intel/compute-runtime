/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/os_interface/windows/registry_reader_tests.h"

#include "test.h"

using namespace NEO;

using RegistryReaderTest = ::testing::Test;

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
