/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "test.h"
#include "runtime/utilities/linux/debug_env_reader.h"

namespace OCLRT {

class DebugEnvReaderTests : public ::testing::Test {
  public:
    void SetUp() override {
        evr = SettingsReader::createOsReader();
        EXPECT_NE(nullptr, evr);
    }
    void TearDown() override {
        delete evr;
    }
    SettingsReader *evr = nullptr;
};

TEST_F(DebugEnvReaderTests, IfVariableIsSetReturnSetValue) {
    int32_t ret;
    std::string retString;
    std::string defaultString = "Default Value";
    std::string setString = "Expected Value";

    const char *testingVariableValue = "1234";
    setenv("TestingVariable", testingVariableValue, 0);
    ret = evr->getSetting("TestingVariable", 1);
    EXPECT_EQ(1234, ret);

    setenv("TestingVariable", setString.c_str(), 1);
    retString = evr->getSetting("TestingVariable", defaultString);
    EXPECT_EQ(0, retString.compare(setString));

    unsetenv("TestingVariable");
    ret = evr->getSetting("TestingVariable", 1);
    EXPECT_EQ(1, ret);
}

TEST_F(DebugEnvReaderTests, IfVariableIsNotSetReturnDefaultValue) {
    int32_t ret;
    std::string retString;
    std::string defaultString = "Default Value";

    unsetenv("TestingVariable");
    ret = evr->getSetting("TestingVariable", 1);
    EXPECT_EQ(1, ret);

    retString = evr->getSetting("TestingVariable", defaultString);
    EXPECT_EQ(0, retString.compare(defaultString));
}

TEST_F(DebugEnvReaderTests, CheckBoolEnvVariable) {
    bool ret;
    bool defaultValue = true;
    bool expectedValue = false;

    setenv("TestingVariable", "0", 0);
    ret = evr->getSetting("TestingVariable", defaultValue);
    EXPECT_EQ(expectedValue, ret);

    unsetenv("TestingVariable");
    ret = evr->getSetting("TestingVariable", defaultValue);
    EXPECT_EQ(defaultValue, ret);
}
} // namespace OCLRT
