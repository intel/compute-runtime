/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/helpers/required_libs_helpers_tests.h"

#include "shared/source/helpers/required_libs_helpers.h"
#include "shared/test/common/helpers/mock_file_io.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/unit_test/mocks/mock_required_libs_helpers.h"

#include "gtest/gtest.h"

#include <string>
#include <unordered_map>

namespace NEO {

TEST_F(RequiredLibsHelpersTest, givenRequiredLibPathWhenOptionalSearchPathsDefinedThenTheyHavePriority) {
    using namespace std::string_view_literals;

    const auto fakeOptionalPaths = std::to_array({"/fake1/path"sv, "/fake2/path"sv});
    MockRequiredLibsHelpers::optionalSearchPaths.reserve(fakeOptionalPaths.size());
    MockRequiredLibsHelpers::optionalSearchPaths.push_back(fakeOptionalPaths[0]);
    MockRequiredLibsHelpers::optionalSearchPaths.push_back(fakeOptionalPaths[1]);

    VariableBackup<FILE *> mockFopenReturnedBkp(&IoFunctions::mockFopenReturned, static_cast<FILE *>(nullptr));

    constexpr auto exampleLibName = "exampleRequiredLib.so.8.7.6";
    const auto exampleLibPath = NEO::joinPath(std::string{fakeOptionalPaths[1]}, exampleLibName);
    writeDataToFile(exampleLibPath.c_str(), "fakeData", false);
    EXPECT_TRUE(virtualFileExists(exampleLibPath));

    RequiredLibsHelpers::LockableSearchPaths cachedSearchPaths;
    std::string reqLibPath;
    bool ret = NEO::RequiredLibsHelpers::getRequiredLibDirPath<MockRequiredLibsHelpers>(exampleLibName, cachedSearchPaths, reqLibPath);
    EXPECT_TRUE(ret);
    EXPECT_EQ(MockRequiredLibsHelpers::getOptionalBinarySearchPathsCalled, 1);
    EXPECT_EQ(MockRequiredLibsHelpers::getDefaultBinarySearchPathsCalled, 0);
    EXPECT_STREQ(reqLibPath.c_str(), fakeOptionalPaths[1].data());
    removeVirtualFile(exampleLibPath);
}

TEST_F(RequiredLibsHelpersTest, givenRequiredLibPathWhenOptionalSearchPathsDoNotHaveTheLibThenDefaultSearchPathsExamined) {
    using namespace std::string_view_literals;

    const auto fakeOptionalPaths = std::to_array({"/fake1/path"sv, "/fake2/path"sv});
    MockRequiredLibsHelpers::optionalSearchPaths.reserve(fakeOptionalPaths.size());
    MockRequiredLibsHelpers::optionalSearchPaths.push_back(fakeOptionalPaths[0]);
    MockRequiredLibsHelpers::optionalSearchPaths.push_back(fakeOptionalPaths[1]);

    const auto fakeDefaultPaths = std::to_array({"/default1/fake/path"sv, "/default2/fake/path"sv});
    MockRequiredLibsHelpers::defaultSearchPaths.reserve(2U);
    MockRequiredLibsHelpers::defaultSearchPaths.push_back(fakeDefaultPaths[0]);
    MockRequiredLibsHelpers::defaultSearchPaths.push_back(fakeDefaultPaths[1]);

    VariableBackup<FILE *> mockFopenReturnedBkp(&IoFunctions::mockFopenReturned, static_cast<FILE *>(nullptr));

    constexpr auto exampleLibName = "exampleRequiredLib.so.8.7.6";
    const auto exampleLibPath = NEO::joinPath(std::string{fakeDefaultPaths[1]}, exampleLibName);
    writeDataToFile(exampleLibPath.c_str(), "fakeData", false);
    EXPECT_TRUE(virtualFileExists(exampleLibPath));

    RequiredLibsHelpers::LockableSearchPaths cachedSearchPaths;
    std::string reqLibPath;
    bool ret = NEO::RequiredLibsHelpers::getRequiredLibDirPath<MockRequiredLibsHelpers>(exampleLibName, cachedSearchPaths, reqLibPath);
    EXPECT_TRUE(ret);
    EXPECT_EQ(MockRequiredLibsHelpers::getOptionalBinarySearchPathsCalled, 1);
    EXPECT_EQ(MockRequiredLibsHelpers::getDefaultBinarySearchPathsCalled, 1);
    EXPECT_STREQ(reqLibPath.c_str(), fakeDefaultPaths[1].data());
    removeVirtualFile(exampleLibPath);
}

TEST_F(RequiredLibsHelpersTest, givenRequiredLibPathWhenOptionalSearchPathsEmptyThenDefaultSearchPathsExamined) {
    using namespace std::string_view_literals;

    const auto fakeDefaultPaths = std::to_array({"/default1/fake/path"sv, "/default2/fake/path"sv});
    MockRequiredLibsHelpers::defaultSearchPaths.reserve(2U);
    MockRequiredLibsHelpers::defaultSearchPaths.push_back(fakeDefaultPaths[0]);
    MockRequiredLibsHelpers::defaultSearchPaths.push_back(fakeDefaultPaths[1]);

    VariableBackup<FILE *> mockFopenReturnedBkp(&IoFunctions::mockFopenReturned, static_cast<FILE *>(nullptr));

    constexpr auto exampleLibName = "exampleRequiredLib.so.8.7.6";
    const auto exampleLibPath = NEO::joinPath(std::string{fakeDefaultPaths[1]}, exampleLibName);
    writeDataToFile(exampleLibPath.c_str(), "fakeData", false);
    EXPECT_TRUE(virtualFileExists(exampleLibPath));

    RequiredLibsHelpers::LockableSearchPaths cachedSearchPaths;
    std::string reqLibPath;
    bool ret = NEO::RequiredLibsHelpers::getRequiredLibDirPath<MockRequiredLibsHelpers>(exampleLibName, cachedSearchPaths, reqLibPath);
    EXPECT_TRUE(ret);
    EXPECT_EQ(MockRequiredLibsHelpers::getOptionalBinarySearchPathsCalled, 1);
    EXPECT_EQ(MockRequiredLibsHelpers::getDefaultBinarySearchPathsCalled, 1);
    EXPECT_STREQ(reqLibPath.c_str(), fakeDefaultPaths[1].data());
    removeVirtualFile(exampleLibPath);
}

TEST_F(RequiredLibsHelpersTest, givenRequiredLibPathWhenBinaryNotFoundInAnySearchPathThenFunctionReturnsFalse) {
    using namespace std::string_view_literals;

    const auto fakeOptionalPaths = std::to_array({"/fake1/path"sv, "/fake2/path"sv});
    MockRequiredLibsHelpers::optionalSearchPaths.reserve(fakeOptionalPaths.size());
    MockRequiredLibsHelpers::optionalSearchPaths.push_back(fakeOptionalPaths[0]);
    MockRequiredLibsHelpers::optionalSearchPaths.push_back(fakeOptionalPaths[1]);

    const auto fakeDefaultPaths = std::to_array({"/default1/fake/path"sv, "/default2/fake/path"sv});
    MockRequiredLibsHelpers::defaultSearchPaths.reserve(2U);
    MockRequiredLibsHelpers::defaultSearchPaths.push_back(fakeDefaultPaths[0]);
    MockRequiredLibsHelpers::defaultSearchPaths.push_back(fakeDefaultPaths[1]);

    VariableBackup<FILE *> mockFopenReturnedBkp(&IoFunctions::mockFopenReturned, static_cast<FILE *>(nullptr));

    constexpr auto exampleLibName = "exampleRequiredLib.so.8.7.6";
    const auto exampleLibPath = NEO::joinPath(std::string{"/no/such/path"}, exampleLibName);

    RequiredLibsHelpers::LockableSearchPaths cachedSearchPaths;
    std::string reqLibPath;
    bool ret = NEO::RequiredLibsHelpers::getRequiredLibDirPath<MockRequiredLibsHelpers>(exampleLibName, cachedSearchPaths, reqLibPath);
    EXPECT_FALSE(ret);
    EXPECT_EQ(MockRequiredLibsHelpers::getOptionalBinarySearchPathsCalled, 1);
    EXPECT_EQ(MockRequiredLibsHelpers::getDefaultBinarySearchPathsCalled, 1);
}

} // namespace NEO
