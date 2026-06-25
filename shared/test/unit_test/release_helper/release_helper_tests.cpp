/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_release_helper.h"

#include "gtest/gtest.h"

using namespace NEO;

using ReleaseHelperSemaphore64Tests = ::testing::Test;

TEST(ReleaseHelperSemaphore64Tests, givenFtrHwSemaphore64SetWhenIsAvailableSemaphore64CalledThenValueFromReleaseSpecificImplementationIsReturned) {
    MockReleaseHelper releaseHelper;
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrHwSemaphore64 = true;

    releaseHelper.isAvailableSemaphore64BaseResult = true;
    EXPECT_TRUE(releaseHelper.isAvailableSemaphore64(hwInfo));

    releaseHelper.isAvailableSemaphore64BaseResult = false;
    EXPECT_FALSE(releaseHelper.isAvailableSemaphore64(hwInfo));
}

TEST(ReleaseHelperSemaphore64Tests, givenNoFtrHwSemaphore64WhenIsAvailableSemaphore64CalledThenFalseReturned) {
    MockReleaseHelper releaseHelper;
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrHwSemaphore64 = false;
    releaseHelper.isAvailableSemaphore64BaseResult = true;

    EXPECT_FALSE(releaseHelper.isAvailableSemaphore64(hwInfo));
}

TEST(ReleaseHelperSemaphore64Tests, givenEnable64BitSemaphoreFlagSetWhenIsAvailableSemaphore64CalledThenFlagValueOverridesEverythingElse) {
    DebugManagerStateRestore restore;
    MockReleaseHelper releaseHelper;
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrHwSemaphore64 = true;
    releaseHelper.isAvailableSemaphore64BaseResult = true;

    debugManager.flags.Enable64BitSemaphore.set(0);
    EXPECT_FALSE(releaseHelper.isAvailableSemaphore64(hwInfo));

    debugManager.flags.Enable64BitSemaphore.set(1);
    EXPECT_TRUE(releaseHelper.isAvailableSemaphore64(hwInfo));
}

TEST(ReleaseHelperSemaphore64Tests, givenEnable64BitSemaphoreFlagSetWhenIsAvailableSemaphore64CalledThenFtrFlagAndReleaseSpecificValueAreIgnored) {
    DebugManagerStateRestore restore;
    MockReleaseHelper releaseHelper;
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrHwSemaphore64 = false;
    releaseHelper.isAvailableSemaphore64BaseResult = false;

    debugManager.flags.Enable64BitSemaphore.set(1);
    EXPECT_TRUE(releaseHelper.isAvailableSemaphore64(hwInfo));
}

using ReleaseHelperPipeControlTests = ::testing::Test;

TEST(ReleaseHelperPipeControlTests, givenOnlyBaseWaRequiredWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredThenOnlyBasicIsRequired) {
    MockReleaseHelper releaseHelper{};
    releaseHelper.isPipeControlPriorToNonPipelinedStateCommandsBaseWARequiredResult = true;
    releaseHelper.isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequiredResult = false;

    EXPECT_EQ((std::pair<bool, bool>{true, false}), releaseHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(*defaultHwInfo, false));
}

TEST(ReleaseHelperPipeControlTests, givenOnlyExtendedWaRequiredWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredThenOnlyExtendedIsRequired) {
    MockReleaseHelper releaseHelper{};
    releaseHelper.isPipeControlPriorToNonPipelinedStateCommandsBaseWARequiredResult = false;
    releaseHelper.isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequiredResult = true;

    EXPECT_EQ((std::pair<bool, bool>{false, true}), releaseHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(*defaultHwInfo, false));
}

TEST(ReleaseHelperPipeControlTests, givenDebugFlagSetTo1WhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredThenExtendedIsForcedToTrue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(1);

    MockReleaseHelper releaseHelper{};
    releaseHelper.isPipeControlPriorToNonPipelinedStateCommandsBaseWARequiredResult = false;
    releaseHelper.isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequiredResult = false;

    EXPECT_EQ((std::pair<bool, bool>{false, true}), releaseHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(*defaultHwInfo, false));
}

TEST(ReleaseHelperPipeControlTests, givenDebugFlagSetTo0WhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredThenExtendedIsForcedToFalse) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(0);

    MockReleaseHelper releaseHelper{};
    releaseHelper.isPipeControlPriorToNonPipelinedStateCommandsBaseWARequiredResult = true;
    releaseHelper.isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequiredResult = true;

    EXPECT_EQ((std::pair<bool, bool>{true, false}), releaseHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(*defaultHwInfo, false));
}

using ReleaseHelperKernelCapabilitiesTests = ::testing::Test;

TEST(ReleaseHelperKernelCapabilitiesTests, GivenRequestForKernelFp16CapabilitiesThenReturnMinMaxAndLoadStoreCapabilitiesWithCapsFromReleaseHelper) {
    MockReleaseHelper releaseHelper;
    releaseHelper.getAdditionalFp16CapsResult = 0u;

    uint32_t fp16Caps = 0u;
    releaseHelper.getKernelFp16AtomicCapabilities(fp16Caps);

    EXPECT_EQ(1u, releaseHelper.getAdditionalFp16CapsCalled);
    EXPECT_EQ(FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps, fp16Caps);
}

TEST(ReleaseHelperKernelCapabilitiesTests, givenAdditionalFp16CapsWhenGettingKernelFp16AtomicCapabilitiesThenAdditionalCapsAreOredIn) {
    MockReleaseHelper releaseHelper;
    releaseHelper.getAdditionalFp16CapsResult = FpAtomicExtFlags::addAtomicCaps;

    uint32_t fp16Caps = 0u;
    releaseHelper.getKernelFp16AtomicCapabilities(fp16Caps);

    EXPECT_EQ(1u, releaseHelper.getAdditionalFp16CapsCalled);
    EXPECT_EQ(FpAtomicExtFlags::addAtomicCaps | FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps, fp16Caps);
}
