/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "neo_aot_platforms.h"

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

DG2TEST_F(CompilerProductHelperDg2Test, givenDg2ConfigsWhenMatchConfigWithRevIdThenProperConfigIsReturned) {
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();

    std::vector<HardwareIpVersion> dg2G10Config = {AOT::DG2_G10_A0, AOT::DG2_G10_A1, AOT::DG2_G10_B0, AOT::DG2_G10_C0};
    std::vector<HardwareIpVersion> dg2G11Config = {AOT::DG2_G11_A0, AOT::DG2_G11_B0, AOT::DG2_G11_B1};

    for (const auto &config : dg2G10Config) {
        EXPECT_EQ(compilerProductHelper.matchRevisionIdWithProductConfig(config, revIdA0), AOT::DG2_G10_A0);
        EXPECT_EQ(compilerProductHelper.matchRevisionIdWithProductConfig(config, revIdA1), AOT::DG2_G10_A1);
        EXPECT_EQ(compilerProductHelper.matchRevisionIdWithProductConfig(config, revIdB0), AOT::DG2_G10_B0);
        EXPECT_EQ(compilerProductHelper.matchRevisionIdWithProductConfig(config, revIdC0), AOT::DG2_G10_C0);
    }
    for (const auto &config : dg2G11Config) {
        EXPECT_EQ(compilerProductHelper.matchRevisionIdWithProductConfig(config, revIdA0), AOT::DG2_G11_A0);
        EXPECT_EQ(compilerProductHelper.matchRevisionIdWithProductConfig(config, revIdB0), AOT::DG2_G11_B0);
        EXPECT_EQ(compilerProductHelper.matchRevisionIdWithProductConfig(config, revIdB1), AOT::DG2_G11_B1);
    }
}
