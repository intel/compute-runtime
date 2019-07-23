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
    cl_uint numEntries = 1;
    cl_device_id devices[1];

    retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_GPU, numEntries, devices,
                            nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clRetainDevice(devices[0]);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clRetainDevice(devices[0]);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_uint theRef;
    retVal = clGetDeviceInfo(devices[0], CL_DEVICE_REFERENCE_COUNT,
                             sizeof(cl_uint), &theRef, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, theRef);
}

TEST_F(clRetainReleaseDeviceTests, GivenRootDeviceWhenReleasingThenReferenceCountIsOne) {
    cl_uint numEntries = 1;
    cl_device_id devices[1];

    retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_GPU, numEntries, devices,
                            nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseDevice(devices[0]);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseDevice(devices[0]);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_uint theRef;
    retVal = clGetDeviceInfo(devices[0], CL_DEVICE_REFERENCE_COUNT,
                             sizeof(cl_uint), &theRef, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, theRef);
}
} // namespace ULT
