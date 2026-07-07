/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {
namespace ult {

TEST(CreateContextTests, givenZeroNumDevicesWhenCreateContextThenReturnsCLInvalidDevice) {
    cl_int errcode = CL_SUCCESS;
    cl_context ctx = clCreateContext(nullptr, 0, nullptr, nullptr, nullptr, &errcode);
    EXPECT_EQ(nullptr, ctx);
    EXPECT_EQ(CL_INVALID_DEVICE, errcode);
}

TEST(CreateContextTests, givenNullDevicesPointerWhenCreateContextThenReturnsCLInvalidDevice) {
    cl_int errcode = CL_SUCCESS;
    cl_context ctx = clCreateContext(nullptr, 1, nullptr, nullptr, nullptr, &errcode);
    EXPECT_EQ(nullptr, ctx);
    EXPECT_EQ(CL_INVALID_DEVICE, errcode);
}

TEST(CreateContextTests, givenNullFuncNotifyWithUserDataWhenCreateContextThenReturnsCLInvalidValue) {
    cl_int errcode = CL_SUCCESS;
    cl_device_id device = nullptr;
    void *userData = reinterpret_cast<void *>(0x1);
    cl_context ctx = clCreateContext(nullptr, 1, &device, nullptr, userData, &errcode);
    EXPECT_EQ(nullptr, ctx);
    EXPECT_EQ(CL_INVALID_VALUE, errcode);
}

TEST(RetainReleaseContextTests, givenNullContextWhenRetainContextThenReturnsCLInvalidContext) {
    auto retVal = clRetainContext(nullptr);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST(RetainReleaseContextTests, givenNullContextWhenReleaseContextThenReturnsCLInvalidContext) {
    auto retVal = clReleaseContext(nullptr);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST(GetContextInfoTests, givenNullContextWhenGetContextInfoThenReturnsCLInvalidContext) {
    auto retVal = clGetContextInfo(nullptr, CL_CONTEXT_NUM_DEVICES, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST(SetContextDestructorCallbackTests, givenNullCallbackWhenSetContextDestructorCallbackThenReturnsCLInvalidValue) {
    auto retVal = clSetContextDestructorCallback(nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST(SetDefaultDeviceCommandQueueTests, givenAnyArgsWhenSetDefaultDeviceCommandQueueThenReturnsCLInvalidOperation) {
    auto retVal = clSetDefaultDeviceCommandQueue(nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
