/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/built_ins/built_in_tests_shared.inl"

#include "gtest/gtest.h"

#include <string>

using namespace NEO;

TEST(BuiltInTestsL0, givenUseBindlessBuiltinInApiDependentModeWhenBinExtensionPassedThenNameHasBindfulPrefix) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(-1);
    EBuiltInOps::Type builtin = EBuiltInOps::CopyBufferToBuffer;
    const std::string extension = ".bin";
    const std::string platformName = "platformName";
    const uint32_t deviceRevId = 123;

    std::string resourceNameGeneric = createBuiltinResourceName(builtin, extension);
    std::string resourceNameForPlatform = createBuiltinResourceName(builtin, extension, platformName);
    std::string resourceNameForPlatformAndStepping = createBuiltinResourceName(builtin, extension, platformName, deviceRevId);

    std::string expectedResourceNameGeneric = "bindful_copy_buffer_to_buffer.builtin_kernel.bin";

    std::string expectedResourceNameForPlatform = platformName.c_str();
    expectedResourceNameForPlatform += "_0_bindful_copy_buffer_to_buffer.builtin_kernel.bin";

    std::string expectedResourceNameForPlatformAndStepping = platformName.c_str();
    expectedResourceNameForPlatformAndStepping += "_";
    expectedResourceNameForPlatformAndStepping += std::to_string(deviceRevId).c_str();
    expectedResourceNameForPlatformAndStepping += "_bindful_copy_buffer_to_buffer.builtin_kernel.bin";

    EXPECT_EQ(0, strcmp(expectedResourceNameGeneric.c_str(), resourceNameGeneric.c_str()));
    EXPECT_EQ(0, strcmp(expectedResourceNameForPlatform.c_str(), resourceNameForPlatform.c_str()));
    EXPECT_EQ(0, strcmp(expectedResourceNameForPlatformAndStepping.c_str(), resourceNameForPlatformAndStepping.c_str()));
}
TEST(BuiltInTestsL0, givenUseBindlessBuiltinDisabledInL0ApiWhenBinExtensionPassedThenNameHasBindfulPrefix) {
    givenUseBindlessBuiltinDisabledWhenBinExtensionPassedThenNameHasBindfulPrefix();
}

TEST(BuiltInTestsL0, givenUseBindlessBuiltinEnabledInL0ApiWhenBinExtensionPassedThenNameHasBindlessPrefix) {
    givenUseBindlessBuiltinEnabledWhenBinExtensionPassedThenNameHasBindlessPrefix();
}
