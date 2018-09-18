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
    const char *getRegKey() const {
        return igdrclRegKey.c_str();
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

TEST_F(RegistryReaderTest, givenRegistryReaderWhenItIsCreatedWithRegKeySpecifiedThenItsRegKeyIsInitializedAccordingly) {
    std::string regKey = "Software\\Intel\\OpenCL";
    TestedRegistryReader registryReader(regKey);
    EXPECT_STREQ("Software\\Intel\\OpenCL", registryReader.getRegKey());
}
