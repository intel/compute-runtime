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

TEST_F(GetDeviceIDsTests, givenDeviceTypeBitfieldContainingGpuWhenGetDeviceIDsThenGpuDevicesAreReturned) {
    const cl_device_type deviceTypes[] = {
        CL_DEVICE_TYPE_CPU | CL_DEVICE_TYPE_GPU | CL_DEVICE_TYPE_ACCELERATOR,
        CL_DEVICE_TYPE_GPU | CL_DEVICE_TYPE_ACCELERATOR,
        CL_DEVICE_TYPE_GPU | CL_DEVICE_TYPE_CUSTOM,
        CL_DEVICE_TYPE_GPU | CL_DEVICE_TYPE_DEFAULT,
        CL_DEVICE_TYPE_ALL};

    const auto expectedNumDevices = static_cast<cl_uint>(platform->getDevices().size());
    ASSERT_GT(expectedNumDevices, 0u);

    for (auto deviceType : deviceTypes) {
        cl_uint numDevices = 0;
        auto retVal = clGetDeviceIDs(static_cast<cl_platform_id>(platform), deviceType, 0, nullptr, &numDevices);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(expectedNumDevices, numDevices);

        cl_device_id device = nullptr;
        retVal = clGetDeviceIDs(static_cast<cl_platform_id>(platform), deviceType, 1, &device, nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(platform->getDevices()[0].get(), device);
    }
}

TEST_F(GetDeviceIDsTests, givenDeviceTypeBitfieldWithoutGpuWhenGetDeviceIDsThenReturnsCLDeviceNotFound) {
    const cl_device_type deviceTypes[] = {
        CL_DEVICE_TYPE_CPU | CL_DEVICE_TYPE_ACCELERATOR,
        CL_DEVICE_TYPE_CPU | CL_DEVICE_TYPE_CUSTOM,
        CL_DEVICE_TYPE_CPU | CL_DEVICE_TYPE_ACCELERATOR | CL_DEVICE_TYPE_CUSTOM};

    for (auto deviceType : deviceTypes) {
        cl_uint numDevices = 1;
        auto retVal = clGetDeviceIDs(static_cast<cl_platform_id>(platform), deviceType, 0, nullptr, &numDevices);
        EXPECT_EQ(CL_DEVICE_NOT_FOUND, retVal);
        EXPECT_EQ(0u, numDevices);
    }
}

TEST_F(GetDeviceIDsTests, givenDeviceTypeBitfieldWithoutGpuAndNullNumDevicesWhenGetDeviceIDsThenDevicesAreNotWritten) {
    auto dummyDevice = reinterpret_cast<cl_device_id>(0x1357);
    cl_device_id device = dummyDevice;

    auto retVal = clGetDeviceIDs(static_cast<cl_platform_id>(platform), CL_DEVICE_TYPE_CPU | CL_DEVICE_TYPE_ACCELERATOR, 1, &device, nullptr);
    EXPECT_EQ(CL_DEVICE_NOT_FOUND, retVal);
    EXPECT_EQ(dummyDevice, device);
}

TEST_F(GetDeviceIDsTests, givenDeviceTypeDefaultCombinedWithNonGpuTypeWhenGetDeviceIDsThenDefaultDeviceIsReturned) {
    cl_uint numDevices = 0;
    auto dummyDevice = reinterpret_cast<cl_device_id>(0x1357);
    cl_device_id device = dummyDevice;

    auto retVal = clGetDeviceIDs(static_cast<cl_platform_id>(platform), CL_DEVICE_TYPE_CPU | CL_DEVICE_TYPE_DEFAULT, 1, &device, &numDevices);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, numDevices);
    EXPECT_EQ(platform->getDevices()[0].get(), device);
}

using CreateSubDevicesTests = Test<OclFixture>;

TEST_F(CreateSubDevicesTests, givenNullDeviceWhenCreateSubDevicesThenReturnsCLInvalidDevice) {
    cl_uint numDevicesRet = 0xdeadbeef;
    auto retVal = clCreateSubDevices(nullptr, nullptr, 0, nullptr, &numDevicesRet);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
    EXPECT_EQ(0xdeadbeefu, numDevicesRet);
}

TEST_F(CreateSubDevicesTests, givenValidDeviceWhenCreateSubDevicesThenNumDevicesRetIsZeroedAndPartitionFails) {
    cl_device_id device = platform->getDevices()[0].get();
    cl_uint numDevicesRet = 0xdeadbeef;
    cl_device_partition_property properties[] = {CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN, CL_DEVICE_AFFINITY_DOMAIN_NUMA, 0};
    auto retVal = clCreateSubDevices(device, properties, 0, nullptr, &numDevicesRet);
    EXPECT_EQ(CL_DEVICE_PARTITION_FAILED, retVal);
    EXPECT_EQ(0u, numDevicesRet);
}

TEST_F(CreateSubDevicesTests, givenNullNumDevicesRetWhenCreateSubDevicesThenPartitionFails) {
    cl_device_id device = platform->getDevices()[0].get();
    auto retVal = clCreateSubDevices(device, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_DEVICE_PARTITION_FAILED, retVal);
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
