/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
