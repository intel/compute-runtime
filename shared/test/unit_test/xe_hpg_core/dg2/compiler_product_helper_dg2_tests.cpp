/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using CompilerProductHelperDg2Test = ::testing::Test;

DG2TEST_F(CompilerProductHelperDg2Test, givenDg2G10WhenGettingHwInfoConfigThenProperConfigIsReturned) {
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();

    hwInfo.platform.usDeviceID = dg2G10DeviceIds[0];
    EXPECT_EQ(0x800040010u, compilerProductHelper.getHwInfoConfig(hwInfo));
}

DG2TEST_F(CompilerProductHelperDg2Test, givenDg2NonG10WhenGettingHwInfoConfigThenProperConfigIsReturned) {
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();

    hwInfo.platform.usDeviceID = dg2G11DeviceIds[0];
    EXPECT_EQ(0x200040010u, compilerProductHelper.getHwInfoConfig(hwInfo));

    hwInfo.platform.usDeviceID = dg2G12DeviceIds[0];
    EXPECT_EQ(0x200040010u, compilerProductHelper.getHwInfoConfig(hwInfo));
}
