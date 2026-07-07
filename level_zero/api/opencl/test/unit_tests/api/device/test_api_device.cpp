/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {
namespace ult {

using GetDeviceIDsTests = Test<OclFixture>;

TEST_F(GetDeviceIDsTests, givenInvalidDeviceTypeWhenGetDeviceIDsThenReturnsCLInvalidDeviceType) {
    auto retVal = clGetDeviceIDs(static_cast<cl_platform_id>(platform), 0, 1, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_DEVICE_TYPE, retVal);
}

TEST_F(GetDeviceIDsTests, givenBothOutputsNullWhenGetDeviceIDsThenReturnsCLInvalidValue) {
    auto retVal = clGetDeviceIDs(static_cast<cl_platform_id>(platform), CL_DEVICE_TYPE_GPU, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(GetDeviceIDsTests, givenZeroNumEntriesWithNonNullDevicesWhenGetDeviceIDsThenReturnsCLInvalidValue) {
    cl_device_id device = nullptr;
    auto retVal = clGetDeviceIDs(static_cast<cl_platform_id>(platform), CL_DEVICE_TYPE_GPU, 0, &device, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(GetDeviceIDsTests, givenCpuDeviceTypeWhenGetDeviceIDsThenReturnsCLDeviceNotFound) {
    cl_uint numDevices = 0;
    auto retVal = clGetDeviceIDs(static_cast<cl_platform_id>(platform), CL_DEVICE_TYPE_CPU, 1, nullptr, &numDevices);
    EXPECT_EQ(CL_DEVICE_NOT_FOUND, retVal);
    EXPECT_EQ(0u, numDevices);
}

TEST_F(GetDeviceIDsTests, givenAcceleratorDeviceTypeWhenGetDeviceIDsThenReturnsCLDeviceNotFound) {
    cl_uint numDevices = 0;
    auto retVal = clGetDeviceIDs(static_cast<cl_platform_id>(platform), CL_DEVICE_TYPE_ACCELERATOR, 1, nullptr, &numDevices);
    EXPECT_EQ(CL_DEVICE_NOT_FOUND, retVal);
    EXPECT_EQ(0u, numDevices);
}

TEST(GetDeviceInfoTests, givenNullDeviceWhenGetDeviceInfoThenReturnsCLInvalidDevice) {
    auto retVal = clGetDeviceInfo(nullptr, CL_DEVICE_TYPE, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST(RetainReleaseDeviceTests, givenNullDeviceWhenRetainDeviceThenReturnsCLInvalidDevice) {
    auto retVal = clRetainDevice(nullptr);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST(RetainReleaseDeviceTests, givenNullDeviceWhenReleaseDeviceThenReturnsCLInvalidDevice) {
    auto retVal = clReleaseDevice(nullptr);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST(GetDeviceAndHostTimerTests, givenNullDeviceWhenGetDeviceAndHostTimerThenReturnsCLInvalidDevice) {
    cl_ulong deviceTs = 0;
    cl_ulong hostTs = 0;
    auto retVal = clGetDeviceAndHostTimer(nullptr, &deviceTs, &hostTs);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST(GetDeviceAndHostTimerTests, givenNullTimestampsWhenGetDeviceAndHostTimerThenReturnsCLInvalidValue) {
    auto retVal = clGetDeviceAndHostTimer(nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST(GetHostTimerTests, givenNullDeviceWhenGetHostTimerThenReturnsCLInvalidDevice) {
    cl_ulong hostTs = 0;
    auto retVal = clGetHostTimer(nullptr, &hostTs);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST(GetHostTimerTests, givenNullTimestampWhenGetHostTimerThenReturnsCLInvalidValue) {
    auto retVal = clGetHostTimer(nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
