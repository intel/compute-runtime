/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/device_factory.h"

#include "test.h"

using namespace NEO;

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
