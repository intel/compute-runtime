/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/debug_env_reader.h"

#include "test.h"

#include <memory>

namespace NEO {

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

TEST_F(DebugEnvReaderTests, appSpecificLacationReturnClCacheLocation) {
    std::string appSpecific;
    appSpecific = "cl_cache_dir";
    EXPECT_EQ(appSpecific, evr->appSpecificLocation(appSpecific));
}

TEST_F(DebugEnvReaderTests, givenEnvironmentVariableReaderWhenCreateOsReaderWithStringThenNotNullPointer) {
    std::unique_ptr<SettingsReader> evr(SettingsReader::createOsReader(false, ""));
    EXPECT_NE(nullptr, evr);
}
} // namespace NEO
