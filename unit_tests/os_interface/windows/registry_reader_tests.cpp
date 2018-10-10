/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/registry_reader.h"
#include "test.h"

using namespace OCLRT;

class TestedRegistryReader : public RegistryReader {
  public:
    TestedRegistryReader(bool userScope) : RegistryReader(userScope){};
    TestedRegistryReader(std::string regKey) : RegistryReader(regKey){};
    HKEY getHkeyType() const {
        return igdrclHkeyType;
    }
    using RegistryReader::getSetting;
    const char *getRegKey() const {
        return registryReadRootKey.c_str();
    }
};

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
    std::string regKey = "regKey";
    std::string defaultKey = "Software\\Intel\\IGFX\\OCL";
    TestedRegistryReader registryReader(regKey);
    EXPECT_STREQ((defaultKey + "\\" + regKey).c_str(), registryReader.getRegKey());
}
