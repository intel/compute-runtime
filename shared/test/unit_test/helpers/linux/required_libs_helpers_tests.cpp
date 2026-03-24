/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/helpers/required_libs_helpers_tests.h"

#include "shared/source/helpers/required_libs_helpers.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/unit_test/mocks/mock_required_libs_helpers.h"

#include "gtest/gtest.h"

#include <string>
#include <unordered_map>

namespace NEO {

TEST_F(RequiredLibsHelpersTest, givenRequiredLibsWhenGettingDefaultSearchPathsThenTheyAreCorrect) {
    auto cachedLdLibraryPaths = RequiredLibsHelpers::LockableSearchPaths{};
    auto pathsSpan = RequiredLibsHelpers::getDefaultBinarySearchPaths();
    EXPECT_EQ(pathsSpan.size(), 8U);
    EXPECT_EQ(pathsSpan[0], "/usr/lib/intel-gpu");
    EXPECT_EQ(pathsSpan[1], "/lib");
    EXPECT_EQ(pathsSpan[2], "/lib64");
    EXPECT_EQ(pathsSpan[3], "/lib/x86_64-linux-gnu");
    EXPECT_EQ(pathsSpan[4], "/usr/lib");
    EXPECT_EQ(pathsSpan[5], "/usr/lib64");
    EXPECT_EQ(pathsSpan[6], "/usr/lib/x86_64-linux-gnu");
    EXPECT_EQ(pathsSpan[7], "/usr/local/lib");
}

TEST_F(RequiredLibsHelpersTest, givenLdLibraryPathEnvUsedWhenSearchingForLibThenTheSpecifiedPathsAreReturned) {
    auto envVal = std::string{};
    auto mockedEnv = std::unordered_map<std::string, std::string>{
        {"LD_LIBRARY_PATH", "/test/path/foo:/test/path/bar"}};
    NEO::IoFunctions::mockableEnvValues = &mockedEnv;

    auto cachedLdLibraryPaths = RequiredLibsHelpers::LockableSearchPaths{};
    auto pathsSpan = RequiredLibsHelpers::getOptionalBinarySearchPaths(cachedLdLibraryPaths);
    EXPECT_EQ(NEO::IoFunctions::mockGetenvCalled, 1U);
    EXPECT_EQ(pathsSpan.size(), 2U);
    EXPECT_EQ(pathsSpan.size(), cachedLdLibraryPaths->size());
    EXPECT_EQ(pathsSpan[0], "/test/path/foo");
    EXPECT_EQ(pathsSpan[1], "/test/path/bar");
    EXPECT_EQ((*cachedLdLibraryPaths)[0], pathsSpan[0]);
    EXPECT_EQ((*cachedLdLibraryPaths)[1], pathsSpan[1]);
}

TEST_F(RequiredLibsHelpersTest, givenPreviouslyCachedRequiredLibsOptionalSearchPathsWhenSearchingAgainThenCachedValuesUsed) {
    auto mockedEnv = std::unordered_map<std::string, std::string>{
        {"LD_LIBRARY_PATH", "/test/path/FOO"}};
    NEO::IoFunctions::mockableEnvValues = &mockedEnv;

    auto cachedLdLibraryPaths = RequiredLibsHelpers::LockableSearchPaths{};
    auto pathsSpan1 = RequiredLibsHelpers::getOptionalBinarySearchPaths(cachedLdLibraryPaths);
    EXPECT_EQ(NEO::IoFunctions::mockGetenvCalled, 1U);
    EXPECT_EQ(pathsSpan1.size(), 1U);
    EXPECT_EQ(pathsSpan1[0], "/test/path/FOO");

    auto pathsSpan2 = RequiredLibsHelpers::getOptionalBinarySearchPaths(cachedLdLibraryPaths);
    EXPECT_EQ(NEO::IoFunctions::mockGetenvCalled, 1U);
    EXPECT_EQ(pathsSpan2.size(), 1U);
    EXPECT_EQ(pathsSpan2[0], "/test/path/FOO");
}

TEST_F(RequiredLibsHelpersTest, givenLdLibraryPathNotSetWhenSearchingRequiredLibsThenOptionalSearchPathsAreEmpty) {
    auto mockedEnv = std::unordered_map<std::string, std::string>{};
    NEO::IoFunctions::mockableEnvValues = &mockedEnv;

    auto cachedLdLibraryPaths = RequiredLibsHelpers::LockableSearchPaths{};
    auto pathsSpan = RequiredLibsHelpers::getOptionalBinarySearchPaths(cachedLdLibraryPaths);
    EXPECT_EQ(NEO::IoFunctions::mockGetenvCalled, 1U);
    EXPECT_TRUE(pathsSpan.empty());
    EXPECT_TRUE(cachedLdLibraryPaths->empty());
}

TEST_F(RequiredLibsHelpersTest, givenLdLibraryPathSetToEmptyStringWhenSearchingRequiredLibsThenOptionalSearchPathsAreEmpty) {
    // this is not natural but handled anyway
    auto mockedEnv = std::unordered_map<std::string, std::string>{
        {"LD_LIBRARY_PATH", ""}};
    NEO::IoFunctions::mockableEnvValues = &mockedEnv;

    auto cachedLdLibraryPaths = RequiredLibsHelpers::LockableSearchPaths{};
    auto pathsSpan = RequiredLibsHelpers::getOptionalBinarySearchPaths(cachedLdLibraryPaths);
    EXPECT_EQ(NEO::IoFunctions::mockGetenvCalled, 1U);
    EXPECT_TRUE(pathsSpan.empty());
    EXPECT_TRUE(cachedLdLibraryPaths->empty());
}

} // namespace NEO
