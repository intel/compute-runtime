/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
