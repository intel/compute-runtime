/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/cl_device/cl_device.h"
#include "level_zero/api/opencl/source/platform/platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

namespace NEO {
namespace LEO {
namespace ult {

using ClDeviceTests = Test<OclFixture>;

TEST_F(ClDeviceTests, givenPlatformWhenGetDevicesThenReturnsNonEmptyList) {
    EXPECT_FALSE(platform->getDevices().empty());
}

TEST_F(ClDeviceTests, givenRootDeviceWhenGetIsSubdeviceThenReturnsFalse) {
    auto &devices = platform->getDevices();
    ASSERT_FALSE(devices.empty());
    EXPECT_FALSE(devices[0]->getIsSubdevice());
}

TEST_F(ClDeviceTests, givenRootDeviceWhenGetL0HandleThenReturnsNonNull) {
    auto &devices = platform->getDevices();
    ASSERT_FALSE(devices.empty());
    EXPECT_NE(nullptr, devices[0]->getL0Handle());
}

TEST_F(ClDeviceTests, givenRootDeviceWhenGetHardwareInfoThenIsAccessible) {
    auto &devices = platform->getDevices();
    ASSERT_FALSE(devices.empty());
    const auto &hwInfo = devices[0]->getHardwareInfo();
    EXPECT_NE(0u, hwInfo.platform.eProductFamily);
}

TEST_F(ClDeviceTests, givenRootDeviceWhenGetSharedDeviceInfoThenHasNonZeroMaxWorkGroupSize) {
    auto &devices = platform->getDevices();
    ASSERT_FALSE(devices.empty());
    const auto &deviceInfo = devices[0]->getSharedDeviceInfo();
    EXPECT_GT(deviceInfo.maxWorkGroupSize, 0u);
}

TEST_F(ClDeviceTests, givenRootDeviceWhenGetPlatformHostTimerResolutionThenReturnsNonNegative) {
    auto &devices = platform->getDevices();
    ASSERT_FALSE(devices.empty());
    EXPECT_GE(devices[0]->getPlatformHostTimerResolution(), 0.0);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
