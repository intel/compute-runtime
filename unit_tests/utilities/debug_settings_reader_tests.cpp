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

#include "test.h"
#include "gtest/gtest.h"
#include "runtime/helpers/file_io.h"
#include "runtime/utilities/debug_settings_reader.h"
#include "runtime/os_interface/debug_settings_manager.h"
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
