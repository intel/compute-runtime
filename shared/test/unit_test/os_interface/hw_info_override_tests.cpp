/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

using HwInfoOverrideTest = ::testing::Test;

HWTEST2_F(HwInfoOverrideTest, givenAnyHwConfigStringWhenPrepareDeviceEnvironmentsForProductFamilyOverrideThenTrueIsReturned, IsAtLeastXeCore) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.HardwareInfoOverride.set("1x2x3");

    MockExecutionEnvironment executionEnvironment{};

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);
    EXPECT_TRUE(success);

    auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();
    EXPECT_EQ(hwInfo->gtSystemInfo.SliceCount, 1u);
    EXPECT_EQ(hwInfo->gtSystemInfo.SubSliceCount, 2u);
    EXPECT_EQ(hwInfo->gtSystemInfo.DualSubSliceCount, 2u);
    EXPECT_EQ(hwInfo->gtSystemInfo.EUCount, 6u);
}

HWTEST2_F(HwInfoOverrideTest, givenBlitterEnableMaskOverrideWhenPrepareDeviceEnvironmentsForProductFamilyOverrideThenCorrectBcsInfoMaskIsReturned, IsAtLeastXeCore) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.BlitterEnableMaskOverride.set(0x6);

    MockExecutionEnvironment executionEnvironment{};

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);
    EXPECT_TRUE(success);

    auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();
    EXPECT_EQ(hwInfo->featureTable.ftrBcsInfo, 0x6);
}

TEST_F(HwInfoOverrideTest, givenMaxSubSlicesSupportedOverrideWhenPrepareDeviceEnvironmentsForProductFamilyOverrideThenMaxSubSlicesSupportedValueIsReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.MaxSubSlicesSupportedOverride.set(128);

    MockExecutionEnvironment executionEnvironment{};

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);
    EXPECT_TRUE(success);

    auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();
    EXPECT_EQ(hwInfo->gtSystemInfo.MaxSubSlicesSupported, 128u);
    EXPECT_EQ(hwInfo->gtSystemInfo.MaxDualSubSlicesSupported, 128u);
}