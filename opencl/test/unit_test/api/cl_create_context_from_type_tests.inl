/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

using namespace NEO;

struct ClCreateContextFromTypeTests : Test<PlatformFixture> {
    cl_int retVal = CL_DEVICE_NOT_AVAILABLE;
};

namespace ULT {
void CL_CALLBACK contextCallBack(const char *, const void *,
                                 size_t, void *) {
}

TEST_F(ClCreateContextFromTypeTests, GivenOnlyGpuDeviceTypeAndReturnValueWhenCreatingContextFromTypeThenContextWithSingleDeviceIsCreated) {
    auto context =
        clCreateContextFromType(nullptr, CL_DEVICE_TYPE_GPU, nullptr, nullptr, &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, context);
    EXPECT_NE(nullptr, context->dispatch.icdDispatch);
    EXPECT_NE(nullptr, context->dispatch.crtDispatch);

    auto pContext = castToObject<Context>(context);
    EXPECT_EQ(1u, pContext->getNumDevices());

    retVal = clReleaseContext(context);
    ASSERT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClCreateContextFromTypeTests, GivenCpuTypeWhenCreatingContextFromTypeThenInvalidValueErrorIsReturned) {
    auto context =
        clCreateContextFromType(nullptr, CL_DEVICE_TYPE_CPU, nullptr, nullptr, &retVal);
    ASSERT_EQ(nullptr, context);
    ASSERT_EQ(CL_DEVICE_NOT_FOUND, retVal);
}

TEST_F(ClCreateContextFromTypeTests, GivenNullCallbackFunctionAndNotNullUserDataWhenCreatingContextFromTypeThenInvalidValueErrorIsReturned) {
    cl_int a;
    auto context =
        clCreateContextFromType(nullptr, CL_DEVICE_TYPE_GPU, nullptr, &a, &retVal);
    ASSERT_EQ(nullptr, context);
    ASSERT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ClCreateContextFromTypeTests, GivenCallbackFunctionWhenCreatingContextFromTypeThenCallSucceeds) {
    auto context = clCreateContextFromType(nullptr, CL_DEVICE_TYPE_GPU, contextCallBack, nullptr, &retVal);
    ASSERT_NE(nullptr, context);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context);
    ASSERT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClCreateContextFromTypeTests, GivenOnlyGpuDeviceTypeWhenCreatingContextFromTypeThenCallSucceeds) {
    auto context =
        clCreateContextFromType(nullptr, CL_DEVICE_TYPE_GPU, nullptr, nullptr, nullptr);

    ASSERT_NE(nullptr, context);
    EXPECT_NE(nullptr, context->dispatch.icdDispatch);
    EXPECT_NE(nullptr, context->dispatch.crtDispatch);

    retVal = clReleaseContext(context);
    ASSERT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClCreateContextFromTypeTests, GivenInvalidContextCreationPropertiesWhenCreatingContextFromTypeThenInvalidPlatformErrorIsReturned) {
    cl_context_properties invalidProperties[3] = {CL_CONTEXT_PLATFORM, (cl_context_properties) nullptr, 0};
    auto context = clCreateContextFromType(invalidProperties, CL_DEVICE_TYPE_GPU, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
    EXPECT_EQ(nullptr, context);
}

TEST_F(ClCreateContextFromTypeTests, GivenNonDefaultPlatformInContextCreationPropertiesWhenCreatingContextFromTypeThenSuccessIsReturned) {
    auto nonDefaultPlatform = std::make_unique<MockPlatform>();
    nonDefaultPlatform->initializeWithNewDevices();
    cl_platform_id nonDefaultPlatformCl = nonDefaultPlatform.get();
    cl_context_properties properties[3] = {CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(nonDefaultPlatformCl), 0};
    auto clContext = clCreateContextFromType(properties, CL_DEVICE_TYPE_GPU, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, clContext);
    auto pContext = castToObject<Context>(clContext);
    for (auto i = 0u; i < nonDefaultPlatform->getNumDevices(); i++) {
        EXPECT_EQ(nonDefaultPlatform->getClDevice(i), pContext->getDevice(i));
    }
    clReleaseContext(clContext);
}

TEST_F(ClCreateContextFromTypeTests, GivenNonDefaultPlatformWithInvalidIcdDispatchInContextCreationPropertiesWhenCreatingContextFromTypeThenInvalidPlatformErrorIsReturned) {
    auto nonDefaultPlatform = std::make_unique<MockPlatform>();
    nonDefaultPlatform->initializeWithNewDevices();
    cl_platform_id nonDefaultPlatformCl = nonDefaultPlatform.get();
    nonDefaultPlatformCl->dispatch.icdDispatch = reinterpret_cast<SDispatchTable *>(nonDefaultPlatform.get());
    cl_context_properties properties[3] = {CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(nonDefaultPlatformCl), 0};
    auto clContext = clCreateContextFromType(properties, CL_DEVICE_TYPE_GPU, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
    EXPECT_EQ(nullptr, clContext);
}

TEST(clCreateContextFromTypeTest, GivenPlatformWithMultipleDevicesWhenCreatingContextFromTypeThenContextContainsAllDevices) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleRootDevices.set(2);

    initPlatform();

    cl_int retVal = CL_INVALID_CONTEXT;

    auto context =
        clCreateContextFromType(nullptr, CL_DEVICE_TYPE_GPU, nullptr, nullptr, &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, context);

    auto pContext = castToObject<Context>(context);
    EXPECT_EQ(2u, pContext->getNumDevices());
    EXPECT_EQ(platform()->getClDevice(0), pContext->getDevice(0));
    EXPECT_EQ(platform()->getClDevice(1), pContext->getDevice(1));

    retVal = clReleaseContext(context);
    ASSERT_EQ(CL_SUCCESS, retVal);
}

} // namespace ULT
