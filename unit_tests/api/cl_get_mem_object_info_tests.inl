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
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/helpers/file_io.h"
#include "runtime/helpers/options.h"
#include "unit_tests/helpers/test_files.h"

using namespace OCLRT;

typedef api_tests clGetMemObjectInfoTests;

namespace ULT {

TEST_F(clGetMemObjectInfoTests, memSize) {
    size_t bufferSize = 16;
    cl_mem buffer = nullptr;

    buffer = clCreateBuffer(
        pContext,
        0,
        bufferSize,
        NULL,
        &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    size_t sizeRet = 0;
    retVal = clGetMemObjectInfo(buffer, CL_MEM_SIZE, sizeof(sizeRet), &sizeRet, NULL);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(bufferSize, sizeRet);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clGetMemObjectInfoTests, memMapCount) {
    size_t bufferSize = 16;
    cl_mem buffer = nullptr;

    cl_queue_properties properties = 0;
    cl_command_queue cmdQ = clCreateCommandQueue(pContext, devices[0], properties, &retVal);

    buffer = clCreateBuffer(
        pContext,
        0,
        bufferSize,
        NULL,
        &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    clEnqueueMapBuffer(
        cmdQ,
        buffer,
        CL_TRUE,
        CL_MAP_WRITE,
        0,
        8,
        0,
        nullptr,
        nullptr,
        nullptr);

    size_t sizeRet = 0;
    retVal = clGetMemObjectInfo(buffer, CL_MEM_MAP_COUNT, sizeof(sizeRet), &sizeRet, NULL);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(1u, sizeRet);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseCommandQueue(cmdQ);
}

TEST_F(clGetMemObjectInfoTests, svmPtr) {
    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        size_t bufferSize = 64;
        cl_mem buffer = nullptr;

        void *ptr = clSVMAlloc(pContext, CL_MEM_READ_WRITE, bufferSize, 64);
        ASSERT_NE(nullptr, ptr);

        buffer = clCreateBuffer(
            pContext,
            CL_MEM_USE_HOST_PTR,
            bufferSize,
            ptr,
            &retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, buffer);

        size_t sizeRet = 0;
        retVal = clGetMemObjectInfo(buffer, CL_MEM_USES_SVM_POINTER, sizeof(sizeRet), &sizeRet, NULL);
        ASSERT_EQ(CL_SUCCESS, retVal);
        ASSERT_EQ(1u, sizeRet);

        retVal = clReleaseMemObject(buffer);
        EXPECT_EQ(CL_SUCCESS, retVal);

        clSVMFree(pContext, ptr);
    }
}
} // namespace ULT
