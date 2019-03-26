/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clCreateContextFromTypeTests;

namespace ULT {
void CL_CALLBACK contextCallBack(const char *, const void *,
                                 size_t, void *) {
}

TEST_F(clCreateContextFromTypeTests, GivenOnlyGpuDeviceTypeAndReturnValueWhenCreatingContextFromTypeThenCallSucceeds) {
    auto context =
        clCreateContextFromType(nullptr, CL_DEVICE_TYPE_GPU, nullptr, nullptr, &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, context);
    EXPECT_NE(nullptr, context->dispatch.icdDispatch);
    EXPECT_NE(nullptr, context->dispatch.crtDispatch);

    retVal = clReleaseContext(context);
    ASSERT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateContextFromTypeTests, GivenCpuTypeWhenCreatingContextFromTypeThenInvalidValueErrorIsReturned) {
    auto context =
        clCreateContextFromType(nullptr, CL_DEVICE_TYPE_CPU, nullptr, nullptr, &retVal);
    ASSERT_EQ(nullptr, context);
    ASSERT_EQ(CL_DEVICE_NOT_FOUND, retVal);
}

TEST_F(clCreateContextFromTypeTests, GivenNullCallbackFunctionAndNotNullUserDataWhenCreatingContextFromTypeThenInvalidValueErrorIsReturned) {
    cl_int a;
    auto context =
        clCreateContextFromType(nullptr, CL_DEVICE_TYPE_GPU, nullptr, &a, &retVal);
    ASSERT_EQ(nullptr, context);
    ASSERT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clCreateContextFromTypeTests, GivenCallbackFunctionWhenCreatingContextFromTypeThenCallSucceeds) {
    auto context = clCreateContextFromType(nullptr, CL_DEVICE_TYPE_GPU, contextCallBack, nullptr, &retVal);
    ASSERT_NE(nullptr, context);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context);
    ASSERT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateContextFromTypeTests, GivenOnlyGpuDeviceTypeWhenCreatingContextFromTypeThenCallSucceeds) {
    auto context =
        clCreateContextFromType(nullptr, CL_DEVICE_TYPE_GPU, nullptr, nullptr, nullptr);

    ASSERT_NE(nullptr, context);
    EXPECT_NE(nullptr, context->dispatch.icdDispatch);
    EXPECT_NE(nullptr, context->dispatch.crtDispatch);

    retVal = clReleaseContext(context);
    ASSERT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateContextFromTypeTests, GivenInvalidContextCreationPropertiesWhenCreatingContextFromTypeThenInvalidPlatformErrorIsReturned) {
    cl_context_properties invalidProperties[3] = {CL_CONTEXT_PLATFORM, (cl_context_properties) nullptr, 0};
    auto context = clCreateContextFromType(invalidProperties, CL_DEVICE_TYPE_GPU, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
    EXPECT_EQ(nullptr, context);
}

} // namespace ULT
