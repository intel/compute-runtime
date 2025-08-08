/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/mocks/ult_cl_device_factory_with_platform.h"

#include "cl_api_tests.h"

using namespace NEO;

using ClGetSupportedImageFormatsTests = ApiTests;

TEST_F(ClGetSupportedImageFormatsTests, GivenValidParamsWhenGettingSupportImageFormatsThenNumImageFormatsIsGreaterThanZero) {
    if (!pContext->getDevice(0)->getSharedDeviceInfo().imageSupport) {
        GTEST_SKIP();
    }
    cl_uint numImageFormats = 0;
    retVal = clGetSupportedImageFormats(
        pContext,
        CL_MEM_READ_WRITE,
        CL_MEM_OBJECT_IMAGE2D,
        0,
        nullptr,
        &numImageFormats);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(numImageFormats, 0u);
}

TEST_F(ClGetSupportedImageFormatsTests, givenInvalidContextWhenGettingSupportImageFormatsThenClInvalidContextErrorIsReturned) {
    auto device = pContext->getDevice(0u);
    auto dummyContext = reinterpret_cast<cl_context>(device);

    cl_uint numImageFormats = 0;
    retVal = clGetSupportedImageFormats(
        dummyContext,
        CL_MEM_READ_WRITE,
        CL_MEM_OBJECT_IMAGE2D,
        0,
        nullptr,
        &numImageFormats);

    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST(clGetSupportedImageFormatsTest, givenPlatforNotSupportingImageWhenGettingSupportImageFormatsThenCLSuccessReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.supportsImages = false;
    UltClDeviceFactoryWithPlatform deviceFactory{1, 0, MockClDevice::prepareExecutionEnvironment(&hwInfo, 0)};
    auto device = deviceFactory.rootDevices[0];
    cl_device_id clDevice = device;
    cl_int retVal;
    auto context = ReleaseableObjectPtr<Context>(Context::create<Context>(nullptr, ClDeviceVector(&clDevice, 1), nullptr, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_uint numImageFormats = 0;
    retVal = clGetSupportedImageFormats(
        context.get(),
        CL_MEM_READ_WRITE,
        CL_MEM_OBJECT_IMAGE2D,
        0,
        nullptr,
        &numImageFormats);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, numImageFormats);
}

TEST(clGetSupportedImageFormatsTest, givenPlatformNotSupportingReadWriteImagesWhenGettingSupportedImageFormatsThenCLSuccessIsReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.supportsImages = true;
    UltClDeviceFactoryWithPlatform deviceFactory{1, 0, MockClDevice::prepareExecutionEnvironment(&hwInfo, 0)};
    auto device = deviceFactory.rootDevices[0];
    cl_device_id clDevice = device;
    cl_int retVal;
    auto context = ReleaseableObjectPtr<Context>(Context::create<Context>(nullptr, ClDeviceVector(&clDevice, 1), nullptr, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_uint numImageFormats = 0;
    retVal = clGetSupportedImageFormats(
        context.get(),
        CL_MEM_KERNEL_READ_AND_WRITE,
        CL_MEM_OBJECT_IMAGE2D,
        0,
        nullptr,
        &numImageFormats);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(numImageFormats, 0u);
}

TEST(clGetSupportedImageFormatsTest, givenPlatforNotSupportingImageAndNullPointerToNumFormatsWhenGettingSupportImageFormatsThenCLSuccessReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.supportsImages = false;
    UltClDeviceFactoryWithPlatform deviceFactory{1, 0, MockClDevice::prepareExecutionEnvironment(&hwInfo, 0)};
    auto device = deviceFactory.rootDevices[0];
    cl_device_id clDevice = device;
    cl_int retVal;
    auto context = ReleaseableObjectPtr<Context>(Context::create<Context>(nullptr, ClDeviceVector(&clDevice, 1), nullptr, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clGetSupportedImageFormats(
        context.get(),
        CL_MEM_READ_WRITE,
        CL_MEM_OBJECT_IMAGE2D,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clGetSupportedImageFormatsTest, givenPlatformWithoutDevicesWhenClGetSupportedImageFormatIsCalledThenDeviceIsTakenFromContext) {
    auto executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto device = std::make_unique<ClDevice>(*Device::create<RootDevice>(executionEnvironment, 0u), platform());
    const DeviceInfo &devInfo = device->getSharedDeviceInfo();
    if (!devInfo.imageSupport) {
        GTEST_SKIP();
    }
    cl_device_id clDevice = device.get();
    cl_int retVal;
    auto context = ReleaseableObjectPtr<MockContext>(MockContext::create<MockContext>(nullptr, ClDeviceVector(&clDevice, 1), nullptr, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(0u, platform()->getNumDevices());
    cl_uint numImageFormats = 0;
    retVal = clGetSupportedImageFormats(
        context.get(),
        CL_MEM_READ_WRITE,
        CL_MEM_OBJECT_IMAGE2D,
        0,
        nullptr,
        &numImageFormats);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(numImageFormats, 0u);
}
