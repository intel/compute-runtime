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

#include "debug_file_reader_tests.inl"

using namespace OCLRT;
using namespace std;

TEST(SettingsFileReader, givenTestFileWithDefaultValuesWhenTheyAreQueriedThenDefaultValuesMatch) {

    // Use test settings file
    std::unique_ptr<TestSettingsFileReader> reader = unique_ptr<TestSettingsFileReader>(new TestSettingsFileReader(TestSettingsFileReader::testPath));
    ASSERT_NE(nullptr, reader);

    size_t debugVariableCount = 0;
    bool compareSuccessful = false;
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)        \
    compareSuccessful = defaultValue == reader->getSetting(#variableName, defaultValue); \
    EXPECT_TRUE(compareSuccessful) << #variableName;                                     \
    debugVariableCount++;
#include "DebugVariables.inl"
#undef DECLARE_DEBUG_VARIABLE

    size_t mapCount = reader->getValueSettingsCount() + reader->getStringSettingsCount();
    EXPECT_EQ(mapCount, debugVariableCount);
}

TEST(SettingsFileReader, GetSetting) {

    // Use test settings file
    std::unique_ptr<TestSettingsFileReader> reader = unique_ptr<TestSettingsFileReader>(new TestSettingsFileReader(TestSettingsFileReader::testPath));
    ASSERT_NE(nullptr, reader);

#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description) \
    {                                                                             \
        dataType defaultData = defaultValue;                                      \
        dataType tempData = reader->getSetting(#variableName, defaultData);       \
        if (tempData == defaultData) {                                            \
            EXPECT_TRUE(true);                                                    \
        }                                                                         \
    }
#include "DebugVariables.inl"
#undef DECLARE_DEBUG_VARIABLE
}