/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/path.h"

#include "gtest/gtest.h"
#include "os_inc.h"

#include <string>

using namespace NEO;

TEST(PathTests, givenOnlyRhsWhenJoinPathThenOnlyRhsIsReturned) {
    const std::string lhs = "";
    const std::string rhs = "folder/file.txt";
    const std::string result = joinPath(lhs, rhs);
    EXPECT_EQ(rhs, result);
}

TEST(PathTests, givenOnlyLhsWhenJoinPathThenOnlyLhsIsReturned) {
    const std::string lhs = "folder";
    const std::string rhs = "";
    const std::string result = joinPath(lhs, rhs);
    EXPECT_EQ(lhs, result);
}

TEST(PathTests, givenLhsEndingWithPathSeparatorAndNonEmptyRhsWhenJoinPathThenLhsAndRhsAreJoined) {
    const std::string lhs = std::string("folder") + PATH_SEPARATOR;
    const std::string rhs = "file.txt";
    const std::string expected = lhs + rhs;
    const std::string result = joinPath(lhs, rhs);
    EXPECT_EQ(expected, result);
}

TEST(PathTests, givenLhsNotEndingWithPathSeparatorAndNonEmptyRhsWhenJoinPathThenPathSeparatorIsAppendedBetweenLhsAndRhs) {
    const std::string lhs = "folder";
    const std::string rhs = "file.txt";
    const std::string expected = std::string("folder") + PATH_SEPARATOR + std::string("file.txt");
    const std::string result = joinPath(lhs, rhs);
    EXPECT_EQ(expected, result);
}
