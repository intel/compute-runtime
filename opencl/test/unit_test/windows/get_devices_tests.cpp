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

#include "opencl/source/platform/platform.h"
#include "test.h"

using namespace NEO;

using GetDevicesTests = ::testing::Test;

HWTEST_F(GetDevicesTests, WhenGetDevicesIsCalledThenSuccessIsReturned) {
    ExecutionEnvironment executionEnvironment;

    auto returnValue = DeviceFactory::getDevices(executionEnvironment);
    EXPECT_EQ(true, returnValue);
}

HWTEST_F(GetDevicesTests, whenGetDevicesIsCalledThenGmmIsBeingInitializedAfterFillingHwInfo) {
    platformsImpl.clear();
    auto executionEnvironment = new ExecutionEnvironment();
    platformsImpl.push_back(std::make_unique<Platform>(*executionEnvironment));
    executionEnvironment->prepareRootDeviceEnvironments(1u);
    auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hwInfo->platform.eProductFamily = PRODUCT_FAMILY::IGFX_UNKNOWN;
    hwInfo->platform.ePCHProductFamily = PCH_PRODUCT_FAMILY::PCH_UNKNOWN;

    auto returnValue = DeviceFactory::getDevices(*executionEnvironment);
    EXPECT_TRUE(returnValue);
}
