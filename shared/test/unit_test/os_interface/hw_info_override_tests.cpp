/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

using HwInfoOverrideTest = ::testing::Test;

HWTEST2_F(HwInfoOverrideTest, givenAnyHwConfigStringWhenPrepareDeviceEnvironmentsForProductFamilyOverrideThenTrueIsReturned, IsAtLeastXeHpCore) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.HardwareInfoOverride.set("1x2x3");

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);
    EXPECT_TRUE(success);

    auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();
    EXPECT_EQ(hwInfo->gtSystemInfo.SliceCount, 1u);
    EXPECT_EQ(hwInfo->gtSystemInfo.SubSliceCount, 2u);
    EXPECT_EQ(hwInfo->gtSystemInfo.DualSubSliceCount, 2u);
    EXPECT_EQ(hwInfo->gtSystemInfo.EUCount, 6u);
}
