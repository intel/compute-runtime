/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/platform/platform.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clRetainReleaseDeviceTests;

namespace ULT {
TEST_F(clRetainReleaseDeviceTests, GivenRootDeviceWhenRetainingThenReferenceCountIsOne) {
    cl_uint numEntries = numRootDevices;
    cl_device_id devices[numRootDevices];

    retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_GPU, numEntries, devices,
                            nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clRetainDevice(devices[testedRootDeviceIndex]);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clRetainDevice(devices[testedRootDeviceIndex]);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_uint theRef;
    retVal = clGetDeviceInfo(devices[testedRootDeviceIndex], CL_DEVICE_REFERENCE_COUNT,
                             sizeof(cl_uint), &theRef, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, theRef);
}

TEST_F(clRetainReleaseDeviceTests, GivenRootDeviceWhenReleasingThenReferenceCountIsOne) {
    constexpr cl_uint numEntries = numRootDevices;
    cl_device_id devices[numRootDevices];

    auto retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_GPU, numEntries, devices,
                                 nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseDevice(devices[testedRootDeviceIndex]);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseDevice(devices[testedRootDeviceIndex]);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_uint theRef;
    retVal = clGetDeviceInfo(devices[testedRootDeviceIndex], CL_DEVICE_REFERENCE_COUNT,
                             sizeof(cl_uint), &theRef, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, theRef);
}
} // namespace ULT
