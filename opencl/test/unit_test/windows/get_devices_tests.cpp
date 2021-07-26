/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "test.h"

namespace NEO {
bool prepareDeviceEnvironments(ExecutionEnvironment &executionEnvironment);

using PrepareDeviceEnvironmentsTests = ::testing::Test;

HWTEST_F(PrepareDeviceEnvironmentsTests, WhenPrepareDeviceEnvironmentsIsCalledThenSuccessIsReturned) {
    ExecutionEnvironment executionEnvironment;

    auto returnValue = DeviceFactory::prepareDeviceEnvironments(executionEnvironment);
    EXPECT_EQ(true, returnValue);
}

HWTEST_F(PrepareDeviceEnvironmentsTests, whenPrepareDeviceEnvironmentsIsCalledThenGmmIsBeingInitializedAfterFillingHwInfo) {
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1u);
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hwInfo->platform.eProductFamily = PRODUCT_FAMILY::IGFX_UNKNOWN;
    hwInfo->platform.ePCHProductFamily = PCH_PRODUCT_FAMILY::PCH_UNKNOWN;
    EXPECT_EQ(nullptr, executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper());

    auto returnValue = DeviceFactory::prepareDeviceEnvironments(executionEnvironment);
    EXPECT_TRUE(returnValue);
    EXPECT_NE(nullptr, executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper());
}

HWTEST_F(PrepareDeviceEnvironmentsTests, Given32bitApplicationWhenPrepareDeviceEnvironmentsIsCalledThenFalseIsReturned) {
    REQUIRE_32BIT_OR_SKIP();
    DebugManagerStateRestore restore;
    DebugManager.flags.NodeOrdinal.set(0); // Dont disable RCS

    NEO::ExecutionEnvironment executionEnviornment;

    auto returnValue = NEO::prepareDeviceEnvironments(executionEnviornment);

    switch (::productFamily) {
    case IGFX_UNKNOWN:
#ifdef SUPPORT_XE_HP_SDV
    case IGFX_XE_HP_SDV:
#endif
        EXPECT_FALSE(returnValue);
        break;
    default:
        EXPECT_TRUE(returnValue);
        break;
    }
}

HWTEST_F(PrepareDeviceEnvironmentsTests, givenRcsAndCcsNotSupportedWhenInitializingThenReturnFalse) {
    REQUIRE_64BIT_OR_SKIP();

    NEO::ExecutionEnvironment executionEnviornment;
    HardwareInfo hwInfo = *defaultHwInfo;

    auto hwInfoConfig = HwInfoConfig::get(hwInfo.platform.eProductFamily);
    hwInfoConfig->configureHardwareCustom(&hwInfo, nullptr);

    bool expectedValue = false;
    if (hwInfo.featureTable.ftrRcsNode || hwInfo.featureTable.ftrCCSNode) {
        expectedValue = true;
    }

    EXPECT_EQ(expectedValue, NEO::prepareDeviceEnvironments(executionEnviornment));
}

HWTEST_F(PrepareDeviceEnvironmentsTests, Given32bitApplicationWhenDebugKeyIsSetThenSupportIsReported) {
    NEO::ExecutionEnvironment executionEnviornment;
    DebugManagerStateRestore restorer;
    DebugManager.flags.Force32BitDriverSupport.set(true);
    EXPECT_TRUE(NEO::prepareDeviceEnvironments(executionEnviornment));
}
} // namespace NEO