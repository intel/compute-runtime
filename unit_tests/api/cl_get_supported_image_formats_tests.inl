/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/utilities/base_object_utils.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "unit_tests/mocks/mock_device.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clGetSupportedImageFormatsTests;

TEST_F(clGetSupportedImageFormatsTests, GivenValidParamsWhenGettingSupportImageFormatsThenNumImageFormatsIsGreaterThanZero) {
    if (!pContext->getDevice(0)->getDeviceInfo().imageSupport) {
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

TEST_F(clGetSupportedImageFormatsTests, givenInvalidContextWhenGettingSupportImageFormatsThenClInvalidContextErrorIsReturned) {
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

TEST(clGetSupportedImageFormatsTest, givenPlatforNotSupportingImageWhenGettingSupportImageFormatsThenClInvalidValueIsReturned) {
    HardwareInfo hwInfo = *platformDevices[0];
    hwInfo.capabilityTable.supportsImages = false;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    cl_device_id clDevice = device.get();
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

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST(clGetSupportedImageFormatsTest, givenPlatformWithoutDevicesWhenClGetSupportedImageFormatIsCalledThenDeviceIsTakenFromContext) {
    auto executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto device = std::make_unique<ClDevice>(*Device::create<RootDevice>(executionEnvironment, 0u));
    const DeviceInfo &devInfo = device->getDeviceInfo();
    if (!devInfo.imageSupport) {
        GTEST_SKIP();
    }
    cl_device_id clDevice = device.get();
    cl_int retVal;
    auto context = ReleaseableObjectPtr<Context>(Context::create<Context>(nullptr, ClDeviceVector(&clDevice, 1), nullptr, nullptr, retVal));
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
