/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"

#include "opencl/test/unit_test/api/cl_api_tests.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/mocks/ult_cl_device_factory_with_platform.h"

using namespace NEO;

using ClCreateContextTests = ApiTests;

namespace ULT {
static int cbInvoked = 0;
void CL_CALLBACK eventCallBack(const char *, const void *,
                               size_t, void *) {
    cbInvoked++;
}

TEST_F(ClCreateContextTests, GivenValidParamsWhenCreatingContextThenContextIsCreated) {
    auto context =
        clCreateContext(nullptr, 1u, &testedClDevice, nullptr, nullptr, &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, context);
    EXPECT_NE(nullptr, context->dispatch.icdDispatch);
    EXPECT_NE(nullptr, context->dispatch.crtDispatch);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClCreateContextTests, GivenNullptrRetValWhenCreatingContextThenContextIsCreated) {
    auto context =
        clCreateContext(nullptr, 1u, &testedClDevice, nullptr, nullptr, nullptr);

    ASSERT_NE(nullptr, context);
    EXPECT_NE(nullptr, context->dispatch.icdDispatch);
    EXPECT_NE(nullptr, context->dispatch.crtDispatch);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClCreateContextTests, GivenZeroDevicesWhenCreatingContextThenInvalidValueErrorIsReturned) {
    auto context = clCreateContext(nullptr, 0, &testedClDevice, nullptr, nullptr, &retVal);
    ASSERT_EQ(nullptr, context);
    ASSERT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ClCreateContextTests, GivenInvalidUserDataWhenCreatingContextThenInvalidValueErrorIsReturned) {
    cl_int someData = 25;
    auto context = clCreateContext(nullptr, 1u, &testedClDevice, nullptr, &someData, &retVal);
    ASSERT_EQ(nullptr, context);
    ASSERT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ClCreateContextTests, GivenInvalidDeviceListWhenCreatingContextThenInvalidDeviceErrorIsReturned) {
    cl_device_id devList[2];
    devList[0] = testedClDevice;
    devList[1] = (cl_device_id)ptrGarbage;

    auto context = clCreateContext(nullptr, 2, devList, nullptr, nullptr, &retVal);
    ASSERT_EQ(nullptr, context);
    ASSERT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(ClCreateContextTests, GivenNullDeviceListWhenCreatingContextThenInvalidValueErrorIsReturned) {
    auto context = clCreateContext(nullptr, 2, nullptr, nullptr, nullptr, &retVal);
    ASSERT_EQ(nullptr, context);
    ASSERT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ClCreateContextTests, GivenNullUserDataWhenCreatingContextThenContextIsCreated) {
    auto context = clCreateContext(nullptr, 1u, &testedClDevice, eventCallBack, nullptr, &retVal);
    ASSERT_NE(nullptr, context);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClCreateContextTests, givenMultipleRootDevicesWithoutSubDevicesWhenCreatingContextThenContextIsCreated) {
    UltClDeviceFactoryWithPlatform deviceFactory{2, 0};
    cl_device_id devices[] = {deviceFactory.rootDevices[0], deviceFactory.rootDevices[1]};
    auto context = clCreateContext(nullptr, 2u, devices, eventCallBack, nullptr, &retVal);
    EXPECT_NE(nullptr, context);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clReleaseContext(context);
}

TEST_F(ClCreateContextTests, givenMultipleSubDevicesFromDifferentRootDevicesWhenCreatingContextThenContextIsCreated) {
    UltClDeviceFactoryWithPlatform deviceFactory{2, 2};
    cl_device_id devices[] = {deviceFactory.subDevices[0], deviceFactory.subDevices[1], deviceFactory.subDevices[2], deviceFactory.subDevices[3]};
    auto context = clCreateContext(nullptr, 4u, devices, eventCallBack, nullptr, &retVal);
    EXPECT_NE(nullptr, context);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clReleaseContext(context);
}

TEST_F(ClCreateContextTests, givenDisabledMultipleRootDeviceSupportWhenCreatingContextThenOutOfHostMemoryErrorIsReturned) {
    UltClDeviceFactoryWithPlatform deviceFactory{2, 2};
    debugManager.flags.EnableMultiRootDeviceContexts.set(false);
    cl_device_id devices[] = {deviceFactory.rootDevices[0], deviceFactory.rootDevices[1]};
    auto context = clCreateContext(nullptr, 2u, devices, eventCallBack, nullptr, &retVal);
    EXPECT_EQ(nullptr, context);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
}

TEST_F(ClCreateContextTests, whenCreateContextWithMultipleRootDevicesWithSubDevicesThenContextIsCreated) {
    UltClDeviceFactoryWithPlatform deviceFactory{2, 2};
    cl_device_id devices[] = {deviceFactory.rootDevices[0], deviceFactory.rootDevices[1]};
    auto context = clCreateContext(nullptr, 2u, devices, eventCallBack, nullptr, &retVal);
    EXPECT_NE(nullptr, context);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clReleaseContext(context);
}

TEST_F(ClCreateContextTests, givenMultipleRootDevicesWhenCreateContextThenRootDeviceIndicesSetIsFilled) {
    UltClDeviceFactoryWithPlatform deviceFactory{3, 2};
    cl_device_id devices[] = {deviceFactory.rootDevices[0], deviceFactory.rootDevices[1], deviceFactory.rootDevices[2]};
    auto context = clCreateContext(nullptr, 3u, devices, eventCallBack, nullptr, &retVal);
    EXPECT_NE(nullptr, context);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pContext = castToObject<Context>(context);
    auto rootDeviceIndices = pContext->getRootDeviceIndices();

    for (auto numDevice = 0u; numDevice < pContext->getNumDevices(); numDevice++) {
        auto rootDeviceIndex = std::find(rootDeviceIndices.begin(), rootDeviceIndices.end(), pContext->getDevice(numDevice)->getRootDeviceIndex());
        EXPECT_EQ(*rootDeviceIndex, pContext->getDevice(numDevice)->getRootDeviceIndex());
    }

    clReleaseContext(context);
}

TEST_F(ClCreateContextTests, givenMultipleRootDevicesWhenCreateContextThenMaxRootDeviceIndexIsProperlyFilled) {
    UltClDeviceFactoryWithPlatform deviceFactory{3, 0};
    debugManager.flags.EnableMultiRootDeviceContexts.set(true);
    cl_device_id devices[] = {deviceFactory.rootDevices[0], deviceFactory.rootDevices[2]};
    auto context = clCreateContext(nullptr, 2u, devices, eventCallBack, nullptr, &retVal);
    EXPECT_NE(nullptr, context);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pContext = castToObject<Context>(context);
    EXPECT_EQ(2u, pContext->getMaxRootDeviceIndex());

    clReleaseContext(context);
}

TEST_F(ClCreateContextTests, givenMultipleRootDevicesWhenCreateContextThenSpecialQueueIsProperlyFilled) {
    UltClDeviceFactoryWithPlatform deviceFactory{3, 0};
    debugManager.flags.EnableMultiRootDeviceContexts.set(true);
    cl_device_id devices[] = {deviceFactory.rootDevices[0], deviceFactory.rootDevices[2]};
    auto context = clCreateContext(nullptr, 2u, devices, eventCallBack, nullptr, &retVal);
    EXPECT_NE(nullptr, context);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pContext = castToObject<Context>(context);
    auto rootDeviceIndices = pContext->getRootDeviceIndices();

    EXPECT_EQ(2u, pContext->getMaxRootDeviceIndex());
    StackVec<CommandQueue *, 1> specialQueues;
    specialQueues.resize(pContext->getMaxRootDeviceIndex());

    for (auto numDevice = 0u; numDevice < pContext->getNumDevices(); numDevice++) {
        auto rootDeviceIndex = std::find(rootDeviceIndices.begin(), rootDeviceIndices.end(), pContext->getDevice(numDevice)->getRootDeviceIndex());
        EXPECT_EQ(*rootDeviceIndex, pContext->getDevice(numDevice)->getRootDeviceIndex());
        EXPECT_EQ(*rootDeviceIndex, pContext->getSpecialQueue(*rootDeviceIndex)->getDevice().getRootDeviceIndex());
        specialQueues[numDevice] = pContext->getSpecialQueue(*rootDeviceIndex);
    }

    EXPECT_EQ(2u, specialQueues.size());

    clReleaseContext(context);
}

TEST_F(ClCreateContextTests, givenInvalidContextCreationPropertiesThenContextCreationFails) {
    cl_context_properties invalidProperties[3] = {CL_CONTEXT_PLATFORM, (cl_context_properties) nullptr, 0};
    auto context = clCreateContext(invalidProperties, 1u, &testedClDevice, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
    EXPECT_EQ(nullptr, context);
}

TEST_F(ClCreateContextTests, GivenNonDefaultPlatformInContextCreationPropertiesWhenCreatingContextThenSuccessIsReturned) {
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

TEST_F(ClCreateContextTests, GivenNonDefaultPlatformWithInvalidIcdDispatchInContextCreationPropertiesWhenCreatingContextThenInvalidPlatformErrorIsReturned) {
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

TEST_F(ClCreateContextTests, GivenDeviceNotAssociatedToPlatformInPropertiesWhenCreatingContextThenInvalidDeviceErrorIsReturned) {
    auto nonDefaultPlatform = std::make_unique<MockPlatform>();
    nonDefaultPlatform->initializeWithNewDevices();
    cl_device_id clDevice = platform()->getClDevice(0);
    cl_platform_id nonDefaultPlatformCl = nonDefaultPlatform.get();
    cl_context_properties properties[3] = {CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(nonDefaultPlatformCl), 0};

    auto clContext = clCreateContext(properties, 1, &clDevice, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
    EXPECT_EQ(nullptr, clContext);
}

TEST_F(ClCreateContextTests, GivenDevicesFromDifferentPlatformsWhenCreatingContextWithoutSpecifiedPlatformThenInvalidDeviceErrorIsReturned) {
    auto platform1 = std::make_unique<MockPlatform>();
    auto platform2 = std::make_unique<MockPlatform>();
    platform1->initializeWithNewDevices();
    platform2->initializeWithNewDevices();
    cl_device_id clDevices[] = {platform1->getClDevice(0), platform2->getClDevice(0)};

    auto clContext = clCreateContext(nullptr, 2, clDevices, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
    EXPECT_EQ(nullptr, clContext);
}

TEST_F(ClCreateContextTests, GivenDevicesFromDifferentPlatformsWhenCreatingContextWithSpecifiedPlatformThenInvalidDeviceErrorIsReturned) {
    auto platform1 = std::make_unique<MockPlatform>();
    auto platform2 = std::make_unique<MockPlatform>();
    platform1->initializeWithNewDevices();
    platform2->initializeWithNewDevices();
    cl_device_id clDevices[] = {platform1->getClDevice(0), platform2->getClDevice(0)};

    cl_platform_id clPlatform = platform1.get();
    cl_context_properties properties[3] = {CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(clPlatform), 0};

    auto clContext = clCreateContext(properties, 2, clDevices, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
    EXPECT_EQ(nullptr, clContext);
}
} // namespace ULT
