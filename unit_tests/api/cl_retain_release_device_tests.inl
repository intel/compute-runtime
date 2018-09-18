/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"
#include "runtime/platform/platform.h"

using namespace OCLRT;

typedef api_tests clRetainReleaseDeviceTests;

namespace ULT {
TEST_F(clRetainReleaseDeviceTests, retain) {
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

TEST_F(clRetainReleaseDeviceTests, release) {
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
