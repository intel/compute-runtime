/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/device_factory.h"
#include "opencl/source/platform/platform.h"
#include "test.h"

using namespace NEO;

using GetDevicesTests = ::testing::Test;

HWTEST_F(GetDevicesTests, WhenGetDevicesIsCalledThenSuccessIsReturned) {
    size_t numDevicesReturned = 0;
    ExecutionEnvironment executionEnvironment;

    auto returnValue = DeviceFactory::getDevices(numDevicesReturned, executionEnvironment);
    EXPECT_EQ(true, returnValue);
}

HWTEST_F(GetDevicesTests, whenGetDevicesIsCalledThenGmmIsBeingInitializedAfterFillingHwInfo) {
    platformsImpl.clear();
    auto executionEnvironment = new ExecutionEnvironment();
    platformsImpl.push_back(std::make_unique<Platform>(*executionEnvironment));
    size_t numDevicesReturned = 0;
    auto hwInfo = executionEnvironment->getMutableHardwareInfo();
    hwInfo->platform.eProductFamily = PRODUCT_FAMILY::IGFX_UNKNOWN;
    hwInfo->platform.ePCHProductFamily = PCH_PRODUCT_FAMILY::PCH_UNKNOWN;

    auto returnValue = DeviceFactory::getDevices(numDevicesReturned, *executionEnvironment);
    EXPECT_TRUE(returnValue);
}
