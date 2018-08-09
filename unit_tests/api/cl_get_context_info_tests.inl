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
#include "runtime/context/context.h"
#include "runtime/device/device.h"

using namespace OCLRT;

typedef api_tests clGetContextInfoTests;

namespace ULT {

TEST_F(clGetContextInfoTests, NumDevices) {
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

TEST_F(clGetContextInfoTests, SizeOfDevices) {
    retVal = clGetContextInfo(
        pContext,
        CL_CONTEXT_DEVICES,
        0,
        nullptr,
        &retSize);

    EXPECT_EQ(1 * sizeof(cl_device_id), retSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clGetContextInfoTests, DeviceIDs) {
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

TEST(clGetContextInfo, NullCommandQueue_returnsError) {
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
