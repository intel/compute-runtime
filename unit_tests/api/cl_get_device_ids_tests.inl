/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/platform/platform.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/variable_backup.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clGetDeviceIDsTests;

namespace ULT {

TEST_F(clGetDeviceIDsTests, GivenZeroNumEntriesWhenGettingDeviceIdsThenNumberOfDevicesIsGreaterThanZero) {
    cl_uint numDevices = 0;

    retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_GPU, 0, nullptr, &numDevices);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(numDevices, (cl_uint)0);
}

TEST_F(clGetDeviceIDsTests, GivenNonNullDevicesWhenGettingDeviceIdsThenDeviceIdIsReturned) {
    cl_uint numEntries = 1;
    cl_device_id pDevices[1];

    retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_GPU, numEntries, pDevices, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clGetDeviceIDsTests, GivenNullPlatformWhenGettingDeviceIdsThenDeviceIdIsReturned) {
    cl_uint numEntries = 1;
    cl_device_id pDevices[1];

    retVal = clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_GPU, numEntries, pDevices, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clGetDeviceIDsTests, GivenInvalidDeviceTypeWhenGettingDeviceIdsThenInvalidDeivceTypeErrorIsReturned) {
    cl_uint numEntries = 1;
    cl_device_id pDevices[1];

    retVal = clGetDeviceIDs(pPlatform, 0x0f00, numEntries, pDevices, nullptr);
    EXPECT_EQ(CL_INVALID_DEVICE_TYPE, retVal);
}

TEST_F(clGetDeviceIDsTests, GivenZeroNumEntriesAndNonNullDevicesWhenGettingDeviceIdsThenInvalidValueErrorIsReturned) {
    cl_device_id pDevices[1];

    retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_GPU, 0, pDevices, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clGetDeviceIDsTests, GivenInvalidPlatformWhenGettingDeviceIdsThenInvalidPlatformErrorIsReturned) {
    cl_uint numEntries = 1;
    cl_device_id pDevices[1];
    uint32_t trash[6] = {0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef};
    cl_platform_id p = reinterpret_cast<cl_platform_id>(trash);

    retVal = clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, numEntries, pDevices, nullptr);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
}

TEST_F(clGetDeviceIDsTests, GivenDeviceTypeAllWhenGettingDeviceIdsThenDeviceIdIsReturned) {
    cl_uint numDevices = 0;
    cl_uint numEntries = 1;
    cl_device_id pDevices[1];

    retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_ALL, numEntries, pDevices, &numDevices);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(numDevices, (cl_uint)0);
}

TEST_F(clGetDeviceIDsTests, GivenDeviceTypeDefaultWhenGettingDeviceIdsThenDeviceIdIsReturned) {
    cl_uint numDevices = 0;
    cl_uint numEntries = 1;
    cl_device_id pDevices[1];

    retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_DEFAULT, numEntries, pDevices, &numDevices);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(numDevices, (cl_uint)0);
}

TEST_F(clGetDeviceIDsTests, GivenDeviceTypeCpuWhenGettingDeviceIdsThenDeviceNotFoundErrorIsReturned) {
    cl_uint numDevices = 0;

    retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_CPU, 0, nullptr, &numDevices);

    EXPECT_EQ(CL_DEVICE_NOT_FOUND, retVal);
    EXPECT_EQ(numDevices, (cl_uint)0);
}

} // namespace ULT
namespace NEO {
extern bool overrideDeviceWithDefaultHardwareInfo;
extern bool overrideCommandStreamReceiverCreation;

} // namespace NEO
