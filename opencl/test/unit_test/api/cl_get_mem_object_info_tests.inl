/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/test_files.h"

#include "opencl/source/context/context.h"

#include "cl_api_tests.h"

using namespace NEO;

using ClGetMemObjectInfoTests = ApiTests;

namespace ULT {

TEST_F(ClGetMemObjectInfoTests, givenValidBufferWhenGettingMemObjectInfoThenCorrectBufferSizeIsReturned) {
    size_t bufferSize = 16;
    cl_mem buffer = nullptr;

    buffer = clCreateBuffer(
        pContext,
        0,
        bufferSize,
        NULL,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, buffer);

    size_t paramValue = 0;
    retVal = clGetMemObjectInfo(buffer, CL_MEM_SIZE, sizeof(paramValue), &paramValue, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(bufferSize, paramValue);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClGetMemObjectInfoTests, givenBufferWithMappedRegionWhenGettingMemObjectInfoThenCorrectMapCountIsReturned) {
    size_t bufferSize = 16;
    cl_mem buffer = nullptr;

    cl_queue_properties properties = 0;
    cl_command_queue cmdQ = clCreateCommandQueue(pContext, testedClDevice, properties, &retVal);

    buffer = clCreateBuffer(
        pContext,
        0,
        bufferSize,
        NULL,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, buffer);

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

    cl_uint paramValue = 0;
    retVal = clGetMemObjectInfo(buffer, CL_MEM_MAP_COUNT, sizeof(paramValue), &paramValue, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, paramValue);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseCommandQueue(cmdQ);
}

TEST_F(ClGetMemObjectInfoTests, givenBufferCreatedFromSvmPointerWhenGettingMemObjectInfoThenClTrueIsReturned) {
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        size_t bufferSize = 64;
        cl_mem buffer = nullptr;

        auto ptr = clSVMAlloc(pContext, CL_MEM_READ_WRITE, bufferSize, 64);
        ASSERT_NE(nullptr, ptr);

        buffer = clCreateBuffer(
            pContext,
            CL_MEM_USE_HOST_PTR,
            bufferSize,
            ptr,
            &retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, buffer);

        cl_bool paramValue = CL_FALSE;
        retVal = clGetMemObjectInfo(buffer, CL_MEM_USES_SVM_POINTER, sizeof(paramValue), &paramValue, nullptr);

        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), paramValue);

        retVal = clReleaseMemObject(buffer);
        EXPECT_EQ(CL_SUCCESS, retVal);

        clSVMFree(pContext, ptr);
    }
}
} // namespace ULT
