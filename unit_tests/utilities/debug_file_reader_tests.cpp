/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/utilities/debug_file_reader_tests.inl"

using namespace NEO;
using namespace std;

TEST(SettingsFileReader, givenTestFileWithDefaultValuesWhenTheyAreQueriedThenDefaultValuesMatch) {

    // Use test settings file
    std::unique_ptr<TestSettingsFileReader> reader =
        unique_ptr<TestSettingsFileReader>(new TestSettingsFileReader(TestSettingsFileReader::testPath));

    ASSERT_NE(nullptr, reader);

    size_t debugVariableCount = 0;
    bool compareSuccessful = false;
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)          \
    compareSuccessful = (defaultValue == reader->getSetting(#variableName, defaultValue)); \
    EXPECT_TRUE(compareSuccessful) << #variableName;                                       \
    debugVariableCount++;
#include "debug_variables.inl"
#undef DECLARE_DEBUG_VARIABLE

    size_t mapCount = reader->getValueSettingsCount() + reader->getStringSettingsCount();
    EXPECT_EQ(mapCount, debugVariableCount);
}

TEST(SettingsFileReader, GetSetting) {

    // Use test settings file
    std::unique_ptr<TestSettingsFileReader> reader =
        unique_ptr<TestSettingsFileReader>(new TestSettingsFileReader(TestSettingsFileReader::testPath));
    ASSERT_NE(nullptr, reader);

#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description) \
    {                                                                             \
        dataType defaultData = defaultValue;                                      \
        dataType tempData = reader->getSetting(#variableName, defaultData);       \
                                                                                  \
        if (tempData == defaultData) {                                            \
            EXPECT_TRUE(true);                                                    \
        }                                                                         \
    }
#include "debug_variables.inl"
#undef DECLARE_DEBUG_VARIABLE
}
