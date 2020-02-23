/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/file_io.h"
#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/helpers/test_files.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clGetMemObjectInfoTests;

namespace ULT {

TEST_F(clGetMemObjectInfoTests, GivenValidBufferWhenGettingMemObjectInfoThenCorrectBufferSizeIsReturned) {
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

    size_t paramValue = 0;
    retVal = clGetMemObjectInfo(buffer, CL_MEM_SIZE, sizeof(paramValue), &paramValue, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(bufferSize, paramValue);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clGetMemObjectInfoTests, GivenBufferWithMappedRegionWhenGettingMemObjectInfoThenCorrectMapCountIsReturned) {
    size_t bufferSize = 16;
    cl_mem buffer = nullptr;

    cl_queue_properties properties = 0;
    cl_command_queue cmdQ = clCreateCommandQueue(pContext, devices[testedRootDeviceIndex], properties, &retVal);

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

    cl_uint paramValue = 0;
    retVal = clGetMemObjectInfo(buffer, CL_MEM_MAP_COUNT, sizeof(paramValue), &paramValue, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(1u, paramValue);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseCommandQueue(cmdQ);
}

TEST_F(clGetMemObjectInfoTests, GivenBufferCreatedFromSvmPointerWhenGettingMemObjectInfoThenClTrueIsReturned) {
    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
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
        ASSERT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, buffer);

        cl_bool paramValue = CL_FALSE;
        retVal = clGetMemObjectInfo(buffer, CL_MEM_USES_SVM_POINTER, sizeof(paramValue), &paramValue, nullptr);

        ASSERT_EQ(CL_SUCCESS, retVal);
        ASSERT_EQ(static_cast<cl_bool>(CL_TRUE), paramValue);

        retVal = clReleaseMemObject(buffer);
        EXPECT_EQ(CL_SUCCESS, retVal);

        clSVMFree(pContext, ptr);
    }
}
} // namespace ULT
