/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/device/device.h"
#include "runtime/context/context.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clGetContextInfoTests;

namespace ULT {

TEST_F(clGetContextInfoTests, GivenContextNumDevicesParamWhenGettingContextInfoThenNumDevicesIsReturned) {
    cl_uint numDevices = 0;

    retVal = clGetContextInfo(
        pContext,
        CL_CONTEXT_NUM_DEVICES,
        sizeof(cl_uint),
        &numDevices,
        nullptr);

    EXPECT_EQ(1u, numDevices);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clGetContextInfoTests, GivenContextWithSingleDeviceAndContextDevicesParamWhenGettingContextInfoThenListOfDevicesContainsOneDevice) {
    retVal = clGetContextInfo(
        pContext,
        CL_CONTEXT_DEVICES,
        0,
        nullptr,
        &retSize);

    EXPECT_EQ(1 * sizeof(cl_device_id), retSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clGetContextInfoTests, GivenContextWithMultipleDevicesAndContextDevicesParamWhenGettingContextInfoThenListOfDevicesContainsAllDevices) {
    auto devicesReturned = new cl_device_id[this->num_devices];
    cl_uint numDevices = this->num_devices;

    auto context = clCreateContext(
        nullptr,
        this->num_devices,
        this->devices,
        nullptr,
        nullptr,
        &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, context);

    retVal = clGetContextInfo(
        context,
        CL_CONTEXT_DEVICES,
        numDevices * sizeof(cl_device_id),
        devicesReturned,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);
    for (size_t deviceOrdinal = 0; deviceOrdinal < this->num_devices; ++deviceOrdinal) {
        EXPECT_EQ(this->devices[deviceOrdinal], devicesReturned[deviceOrdinal]);
    }

    clReleaseContext(context);
    delete[] devicesReturned;
}

TEST(clGetContextInfo, GivenNullContextWhenGettingContextInfoThenInvalidContextErrorIsReturned) {
    cl_device_id pDevices[1];
    cl_uint numDevices = 1;

    auto retVal = clGetContextInfo(
        nullptr,
        CL_CONTEXT_DEVICES,
        numDevices * sizeof(cl_device_id),
        pDevices,
        nullptr);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}
} // namespace ULT
