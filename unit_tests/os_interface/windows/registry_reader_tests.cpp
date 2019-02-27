/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/os_interface/windows/registry_reader_tests.h"

#include "test.h"

using namespace OCLRT;

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
