/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/device_factory.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/source/xe_hpg_core/hw_info_dg2.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "neo_aot_platforms.h"

using namespace NEO;

using Dg2DeviceFactoryTest = ::testing::Test;

DG2TEST_F(Dg2DeviceFactoryTest, givenOverrideHwIpVersionWhenPrepareDeviceEnvironmentsForProductFamilyThenCorrectValuesAreSet) {
    DebugManagerStateRestore stateRestore;
    MockExecutionEnvironment executionEnvironment{};

    std::vector<std::pair<uint32_t, int>> dg2G10Configs = {
        {AOT::DG2_G10_A0, revIdA0},
        {AOT::DG2_G10_A1, revIdA1}};

    for (const auto &[config, revisionID] : dg2G10Configs) {
        debugManager.flags.OverrideHwIpVersion.set(config);

        bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);
        EXPECT_TRUE(success);
        EXPECT_EQ(executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->ipVersion.value, config);
        EXPECT_EQ(executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->platform.usRevId, revisionID);
        EXPECT_EQ(executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->platform.usDeviceID, dg2G10DeviceIds.front());
    }
}

DG2TEST_F(Dg2DeviceFactoryTest, givenOverrideHwIpVersionAndDeviceIdWhenPrepareDeviceEnvironmentsForProductFamilyThenCorrectValuesAreSet) {
    DebugManagerStateRestore stateRestore;
    MockExecutionEnvironment executionEnvironment{};

    std::vector<std::pair<uint32_t, int>> dg2G10Configs = {
        {AOT::DG2_G10_A0, revIdA0},
        {AOT::DG2_G10_A1, revIdA1}};

    debugManager.flags.ForceDeviceId.set("0x1234");
    for (const auto &[config, revisionID] : dg2G10Configs) {
        debugManager.flags.OverrideHwIpVersion.set(config);

        bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);
        EXPECT_TRUE(success);
        EXPECT_EQ(executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->ipVersion.value, config);
        EXPECT_EQ(executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->platform.usRevId, revisionID);
        EXPECT_EQ(executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->platform.usDeviceID, 0x1234u);
    }
}
