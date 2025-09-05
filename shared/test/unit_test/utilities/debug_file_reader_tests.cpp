/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/utilities/debug_file_reader_tests.inl"

using namespace NEO;

TEST(SettingsFileReader, givenTestFileWithDefaultValuesWhenTheyAreQueriedThenDefaultValuesMatch) {

    // Use test settings file
    std::unique_ptr<TestSettingsFileReader> reader =
        std::unique_ptr<TestSettingsFileReader>(new TestSettingsFileReader(TestSettingsFileReader::getTestPath().c_str()));

    ASSERT_NE(nullptr, reader);

    size_t debugVariableCount = 0;
    bool variableFound = false;
    bool compareSuccessful = false;
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)          \
    variableFound = reader->hasSetting(#variableName);                                     \
    EXPECT_TRUE(variableFound) << #variableName;                                           \
    compareSuccessful = (defaultValue == reader->getSetting(#variableName, defaultValue)); \
    EXPECT_TRUE(compareSuccessful) << #variableName;                                       \
    debugVariableCount++;
#define DECLARE_DEBUG_SCOPED_V(dataType, variableName, defaultValue, description, ...) \
    DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)
#include "debug_variables.inl"
#define DECLARE_RELEASE_VARIABLE(dataType, variableName, defaultValue, description) DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)
#define DECLARE_RELEASE_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) DECLARE_RELEASE_VARIABLE(dataType, variableName, defaultValue, description)
#include "release_variables.inl"
#undef DECLARE_RELEASE_VARIABLE_OPT
#undef DECLARE_RELEASE_VARIABLE
#undef DECLARE_DEBUG_SCOPED_V
#undef DECLARE_DEBUG_VARIABLE

    size_t mapCount = reader->getStringSettingsCount();
    EXPECT_EQ(mapCount, debugVariableCount);
}
