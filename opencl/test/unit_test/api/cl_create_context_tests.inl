/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

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
        clCreateContext(nullptr, 1u, &testedClDevice, nullptr, nullptr, &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, context);
    EXPECT_NE(nullptr, context->dispatch.icdDispatch);
    EXPECT_NE(nullptr, context->dispatch.crtDispatch);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateContextTests, noRet) {
    auto context =
        clCreateContext(nullptr, 1u, &testedClDevice, nullptr, nullptr, nullptr);

    ASSERT_NE(nullptr, context);
    EXPECT_NE(nullptr, context->dispatch.icdDispatch);
    EXPECT_NE(nullptr, context->dispatch.crtDispatch);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateContextTests, returnsFail) {
    auto context =
        clCreateContext(nullptr, 0, &testedClDevice, nullptr, nullptr, &retVal);
    ASSERT_EQ(nullptr, context);
    ASSERT_EQ(CL_INVALID_VALUE, retVal);

    cl_int someData = 25;
    context = clCreateContext(nullptr, 1u, &testedClDevice, nullptr, &someData, &retVal);
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
    auto context = clCreateContext(nullptr, 1u, &testedClDevice, eventCallBack, nullptr, &retVal);
    ASSERT_NE(nullptr, context);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateContextTests, givenInvalidContextCreationPropertiesThenContextCreationFails) {
    cl_context_properties invalidProperties[3] = {CL_CONTEXT_PLATFORM, (cl_context_properties) nullptr, 0};
    auto context = clCreateContext(invalidProperties, 1u, &testedClDevice, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
    EXPECT_EQ(nullptr, context);
}

TEST_F(clCreateContextTests, GivenNonDefaultPlatformInContextCreationPropertiesWhenCreatingContextThenSuccessIsReturned) {
    auto nonDefaultPlatform = std::make_unique<MockPlatform>();
    nonDefaultPlatform->initializeWithNewDevices();
    cl_platform_id nonDefaultPlatformCl = nonDefaultPlatform.get();
    cl_device_id clDevice = nonDefaultPlatform->getClDevice(0);
    cl_context_properties properties[3] = {CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(nonDefaultPlatformCl), 0};
    auto clContext = clCreateContext(properties, 1, &clDevice, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, clContext);
    clReleaseContext(clContext);
}

TEST_F(clCreateContextFromTypeTests, GivenNonDefaultPlatformWithInvalidIcdDispatchInContextCreationPropertiesWhenCreatingContextThenInvalidPlatformErrorIsReturned) {
    auto nonDefaultPlatform = std::make_unique<MockPlatform>();
    nonDefaultPlatform->initializeWithNewDevices();
    cl_platform_id nonDefaultPlatformCl = nonDefaultPlatform.get();
    nonDefaultPlatformCl->dispatch.icdDispatch = reinterpret_cast<SDispatchTable *>(nonDefaultPlatform.get());
    cl_device_id clDevice = nonDefaultPlatform->getClDevice(0);
    cl_context_properties properties[3] = {CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(nonDefaultPlatformCl), 0};
    auto clContext = clCreateContext(properties, 1, &clDevice, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
    EXPECT_EQ(nullptr, clContext);
}

} // namespace ClCreateContextTests
