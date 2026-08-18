/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/helpers/cl_to_l0_handles.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {
namespace ult {

using ConvertToZeDeviceHandleTests = Test<OclFixture>;

TEST_F(ConvertToZeDeviceHandleTests, givenValidDeviceWhenConvertingThenUnderlyingL0HandleIsReturned) {
    auto &devices = platform->getDevices();
    ASSERT_FALSE(devices.empty());

    auto clDevice = devices[0].get();
    cl_device_id clDeviceId = clDevice;

    EXPECT_EQ(clDevice->getL0Handle(), ConvertTo::zeDeviceHandle(clDeviceId));
}

TEST_F(ConvertToZeDeviceHandleTests, givenNullDeviceWhenConvertingThenNullIsReturned) {
    EXPECT_EQ(nullptr, ConvertTo::zeDeviceHandle(nullptr));
}

TEST_F(ConvertToZeDeviceHandleTests, givenForeignHandleWhenConvertingThenNullIsReturned) {
    auto &devices = platform->getDevices();
    ASSERT_FALSE(devices.empty());

    cl_device_id foreignHandle = reinterpret_cast<cl_device_id>(platform);
    EXPECT_EQ(nullptr, ConvertTo::zeDeviceHandle(foreignHandle));
}

} // namespace ult
} // namespace LEO
} // namespace NEO
