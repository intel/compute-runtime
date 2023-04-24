/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"

#include "opencl/source/context/context.h"

#include "cl_api_tests.h"

using namespace NEO;

using ClGetContextInfoTests = ApiTests;

namespace ULT {

TEST_F(ClGetContextInfoTests, GivenContextNumDevicesParamWhenGettingContextInfoThenNumDevicesIsReturned) {
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

TEST_F(ClGetContextInfoTests, GivenContextWithSingleDeviceAndContextDevicesParamWhenGettingContextInfoThenListOfDevicesContainsOneDevice) {
    retVal = clGetContextInfo(
        pContext,
        CL_CONTEXT_DEVICES,
        0,
        nullptr,
        &retSize);

    EXPECT_EQ(1 * sizeof(cl_device_id), retSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClGetContextInfoTests, GivenContextWithMultipleDevicesAndContextDevicesParamWhenGettingContextInfoThenListOfDevicesContainsAllDevices) {
    cl_uint numDevices = 2u;
    auto inputDevices = std::make_unique<cl_device_id[]>(numDevices);
    auto outputDevices = std::make_unique<cl_device_id[]>(numDevices);

    for (auto i = 0u; i < numDevices; i++) {
        inputDevices[i] = testedClDevice;
    }

    auto context = clCreateContext(
        nullptr,
        numDevices,
        inputDevices.get(),
        nullptr,
        nullptr,
        &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, context);

    retVal = clGetContextInfo(
        context,
        CL_CONTEXT_DEVICES,
        numDevices * sizeof(cl_device_id),
        outputDevices.get(),
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);
    for (size_t deviceOrdinal = 0; deviceOrdinal < numDevices; ++deviceOrdinal) {
        EXPECT_EQ(inputDevices[deviceOrdinal], outputDevices[deviceOrdinal]);
    }

    clReleaseContext(context);
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
