/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_compiler_release_helper.h"

#include "gtest/gtest.h"

using namespace NEO;
using CompilerReleaseHelperSemaphore64Tests = ::testing::Test;

TEST(CompilerReleaseHelperSemaphore64Tests, givenFtrHwSemaphore64SetWhenIsAvailableSemaphore64CalledThenValueFromReleaseSpecificImplementationIsReturned) {
    MockCompilerReleaseHelper compilerReleaseHelper;
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrHwSemaphore64 = true;

    hwInfo.caps.availableSemaphore64 = true;
    EXPECT_TRUE(compilerReleaseHelper.isAvailableSemaphore64(hwInfo));

    hwInfo.caps.availableSemaphore64 = false;
    EXPECT_FALSE(compilerReleaseHelper.isAvailableSemaphore64(hwInfo));
}

TEST(CompilerReleaseHelperSemaphore64Tests, givenNoFtrHwSemaphore64WhenIsAvailableSemaphore64CalledThenFalseReturned) {
    MockCompilerReleaseHelper compilerReleaseHelper;
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrHwSemaphore64 = false;
    hwInfo.caps.availableSemaphore64 = true;

    EXPECT_FALSE(compilerReleaseHelper.isAvailableSemaphore64(hwInfo));
}

TEST(CompilerReleaseHelperSemaphore64Tests, givenEnable64BitSemaphoreFlagSetWhenIsAvailableSemaphore64CalledThenFlagValueOverridesEverythingElse) {
    DebugManagerStateRestore restore;
    MockCompilerReleaseHelper compilerReleaseHelper;
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrHwSemaphore64 = true;
    hwInfo.caps.availableSemaphore64 = true;

    debugManager.flags.Enable64BitSemaphore.set(0);
    EXPECT_FALSE(compilerReleaseHelper.isAvailableSemaphore64(hwInfo));

    debugManager.flags.Enable64BitSemaphore.set(1);
    EXPECT_TRUE(compilerReleaseHelper.isAvailableSemaphore64(hwInfo));
}

TEST(CompilerReleaseHelperSemaphore64Tests, givenEnable64BitSemaphoreFlagSetWhenIsAvailableSemaphore64CalledThenFtrFlagAndReleaseSpecificValueAreIgnored) {
    DebugManagerStateRestore restore;
    MockCompilerReleaseHelper compilerReleaseHelper;
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrHwSemaphore64 = false;
    hwInfo.caps.availableSemaphore64 = false;

    debugManager.flags.Enable64BitSemaphore.set(1);
    EXPECT_TRUE(compilerReleaseHelper.isAvailableSemaphore64(hwInfo));
}
