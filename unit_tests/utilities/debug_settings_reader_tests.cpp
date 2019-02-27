/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/file_io.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/utilities/debug_settings_reader.h"
#include "test.h"

#include "gtest/gtest.h"

#include <fstream>
#include <string>

using namespace OCLRT;
using namespace std;

TEST(SettingsReader, Create) {
    SettingsReader *reader = SettingsReader::create();
    EXPECT_NE(nullptr, reader);
    delete reader;
}

TEST(SettingsReader, CreateFileReader) {
    bool settingsFileExists = fileExists(SettingsReader::settingsFileName);
    if (!settingsFileExists) {
        char data = 0;
        writeDataToFile(SettingsReader::settingsFileName, &data, 0);
    }
    SettingsReader *reader = SettingsReader::createFileReader();
    EXPECT_NE(nullptr, reader);

    if (!settingsFileExists) {
        remove(SettingsReader::settingsFileName);
    }
    delete reader;
}

TEST(SettingsReader, CreateOsReader) {
    SettingsReader *reader = SettingsReader::createOsReader(false);
    EXPECT_NE(nullptr, reader);
    delete reader;
}

TEST(SettingsReader, CreateOsReaderWithRegKey) {
    std::string regKey = "Software\\Intel\\OpenCL";
    unique_ptr<SettingsReader> reader(SettingsReader::createOsReader(regKey));
    EXPECT_NE(nullptr, reader);
}

TEST(SettingsReader, givenPrintDebugStringWhenCalledWithTrueItPrintsToOutput) {
    int i = 4;
    testing::internal::CaptureStdout();
    printDebugString(true, stdout, "testing error %d", i);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STRNE(output.c_str(), "");
}
TEST(SettingsReader, givenPrintDebugStringWhenCalledWithFalseThenNothingIsPrinted) {
    int i = 4;
    testing::internal::CaptureStdout();
    printDebugString(false, stderr, "Error String %d", i);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STREQ(output.c_str(), "");
}
