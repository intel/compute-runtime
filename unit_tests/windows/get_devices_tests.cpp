/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/hw_info.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/os_interface/device_factory.h"
#include "runtime/platform/platform.h"
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
    platformImpl.reset(new Platform());
    size_t numDevicesReturned = 0;
    HardwareInfo hwInfo;
    hwInfo.platform.eProductFamily = PRODUCT_FAMILY::IGFX_UNKNOWN;
    hwInfo.platform.eRenderCoreFamily = GFXCORE_FAMILY::IGFX_UNKNOWN_CORE;
    hwInfo.platform.ePCHProductFamily = PCH_PRODUCT_FAMILY::PCH_UNKNOWN;
    platform()->peekExecutionEnvironment()->setHwInfo(&hwInfo);

    auto returnValue = DeviceFactory::getDevices(numDevicesReturned, *platform()->peekExecutionEnvironment());
    EXPECT_TRUE(returnValue);
}
