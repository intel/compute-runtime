/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(CompilerReleaseHelperCreateTest, givenArchitectureWithoutRegisteredReleaseTableWhenCreatingCompilerReleaseHelperThenNullptrIsReturned) {
    HardwareIpVersion ipVersion{};
    ipVersion.architecture = compilerReleaseMaxArchitecture - 1;
    ipVersion.release = 0;

    ASSERT_EQ(nullptr, compilerReleaseHelperFactory[ipVersion.architecture]);
    EXPECT_EQ(nullptr, CompilerReleaseHelper::create(ipVersion));
}

TEST(CompilerReleaseHelperCreateTest, givenRegisteredArchitectureButUnregisteredReleaseWhenCreatingCompilerReleaseHelperThenNullptrIsReturned) {
    HardwareIpVersion ipVersion{};
    ipVersion.architecture = 12;
    ipVersion.release = 5;

    ASSERT_NE(nullptr, compilerReleaseHelperFactory[ipVersion.architecture]);
    ASSERT_EQ(nullptr, compilerReleaseHelperFactory[ipVersion.architecture][ipVersion.release]);
    EXPECT_EQ(nullptr, CompilerReleaseHelper::create(ipVersion));
}
