/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/ptr_math.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clCreateContextTests;

namespace ClCreateContextTests {
static int cbInvoked = 0;
void CL_CALLBACK eventCallBack(const char *, const void *,
                               size_t, void *) {
    cbInvoked++;
}

TEST_F(clCreateContextTests, returnsSuccess) {
    auto context =
        clCreateContext(nullptr, num_devices, devices, nullptr, nullptr, &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, context);
    EXPECT_NE(nullptr, context->dispatch.icdDispatch);
    EXPECT_NE(nullptr, context->dispatch.crtDispatch);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateContextTests, noRet) {
    auto context =
        clCreateContext(nullptr, num_devices, devices, nullptr, nullptr, nullptr);

    ASSERT_NE(nullptr, context);
    EXPECT_NE(nullptr, context->dispatch.icdDispatch);
    EXPECT_NE(nullptr, context->dispatch.crtDispatch);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateContextTests, returnsFail) {
    auto context =
        clCreateContext(nullptr, 0, devices, nullptr, nullptr, &retVal);
    ASSERT_EQ(nullptr, context);
    ASSERT_EQ(CL_INVALID_VALUE, retVal);

    cl_int someData = 25;
    context = clCreateContext(nullptr, num_devices, devices, nullptr, &someData, &retVal);
    ASSERT_EQ(nullptr, context);
    ASSERT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clCreateContextTests, invalidDevices) {
    cl_device_id devList[2];
    devList[0] = devices[testedRootDeviceIndex];
    devList[1] = (cl_device_id)ptrGarbage;

    auto context = clCreateContext(nullptr, 2, devList, nullptr, nullptr, &retVal);
    ASSERT_EQ(nullptr, context);
    ASSERT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(clCreateContextTests, nullDevices) {
    auto context = clCreateContext(nullptr, 2, nullptr, nullptr, nullptr, &retVal);
    ASSERT_EQ(nullptr, context);
    ASSERT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clCreateContextTests, nullUserData) {
    auto context = clCreateContext(nullptr, num_devices, devices, eventCallBack, nullptr, &retVal);
    ASSERT_NE(nullptr, context);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateContextTests, givenInvalidContextCreationPropertiesThenContextCreationFails) {
    cl_context_properties invalidProperties[3] = {CL_CONTEXT_PLATFORM, (cl_context_properties) nullptr, 0};
    auto context = clCreateContext(invalidProperties, num_devices, devices, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
    EXPECT_EQ(nullptr, context);
}

} // namespace ClCreateContextTests
