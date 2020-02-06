/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/device/device.h"
#include "core/helpers/get_info.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/api/api.h"
#include "runtime/platform/platform.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_device.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace ::testing;

namespace NEO {
TEST(DeviceOsTest, GivenDefaultDeviceWhenCheckingForOsSpecificExtensionsThenCorrectExtensionsAreSet) {
    auto hwInfo = *platformDevices;
    auto pDevice = MockDevice::createWithNewExecutionEnvironment<Device>(hwInfo);

    std::string extensionString(pDevice->getDeviceInfo().deviceExtensions);

    EXPECT_THAT(extensionString, Not(testing::HasSubstr(std::string("cl_intel_dx9_media_sharing "))));
    EXPECT_THAT(extensionString, Not(testing::HasSubstr(std::string("cl_khr_dx9_media_sharing "))));
    EXPECT_THAT(extensionString, Not(testing::HasSubstr(std::string("cl_khr_d3d10_sharing "))));
    EXPECT_THAT(extensionString, Not(testing::HasSubstr(std::string("cl_khr_d3d11_sharing "))));
    EXPECT_THAT(extensionString, Not(testing::HasSubstr(std::string("cl_intel_d3d11_nv12_media_sharing "))));
    EXPECT_THAT(extensionString, Not(testing::HasSubstr(std::string("cl_intel_simultaneous_sharing "))));

    delete pDevice;
}

TEST(DeviceOsTest, GivenDefaultClDeviceWhenCheckingForOsSpecificExtensionsThenCorrectExtensionsAreSet) {
    auto hwInfo = *platformDevices;
    auto pDevice = MockDevice::createWithNewExecutionEnvironment<Device>(hwInfo);
    auto pClDevice = new ClDevice{*pDevice, platform()};

    std::string extensionString(pClDevice->getDeviceInfo().deviceExtensions);

    EXPECT_THAT(extensionString, Not(testing::HasSubstr(std::string("cl_intel_dx9_media_sharing "))));
    EXPECT_THAT(extensionString, Not(testing::HasSubstr(std::string("cl_khr_dx9_media_sharing "))));
    EXPECT_THAT(extensionString, Not(testing::HasSubstr(std::string("cl_khr_d3d10_sharing "))));
    EXPECT_THAT(extensionString, Not(testing::HasSubstr(std::string("cl_khr_d3d11_sharing "))));
    EXPECT_THAT(extensionString, Not(testing::HasSubstr(std::string("cl_intel_d3d11_nv12_media_sharing "))));
    EXPECT_THAT(extensionString, Not(testing::HasSubstr(std::string("cl_intel_simultaneous_sharing "))));

    delete pClDevice;
}

TEST(DeviceOsTest, supportedSimultaneousInterops) {
    auto pDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));

    std::vector<unsigned int> expected = {0};

    EXPECT_TRUE(pDevice->simultaneousInterops == expected);
}

TEST(DeviceOsTest, DeviceCreationFail) {
    auto hwInfo = *platformDevices;
    auto pDevice = MockDevice::createWithNewExecutionEnvironment<FailDevice>(hwInfo);

    EXPECT_THAT(pDevice, nullptr);
}

TEST(ApiOsTest, notSupportedApiTokens) {
    MockContext context;
    MockBuffer buffer;

    cl_bool boolVal;
    size_t size;
    auto retVal = context.getInfo(CL_CONTEXT_D3D10_PREFER_SHARED_RESOURCES_KHR, sizeof(cl_bool), &boolVal, &size);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    void *paramVal = nullptr;
    retVal = buffer.getMemObjectInfo(CL_MEM_D3D10_RESOURCE_KHR, sizeof(void *), paramVal, &size);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST(ApiOsTest, notSupportedApiList) {
    MockContext context;

    EXPECT_EQ(nullptr, context.dispatch.crtDispatch->clGetDeviceIDsFromDX9INTEL);
    EXPECT_EQ(nullptr, context.dispatch.crtDispatch->clCreateFromDX9MediaSurfaceINTEL);
    EXPECT_EQ(nullptr, context.dispatch.crtDispatch->clEnqueueAcquireDX9ObjectsINTEL);
    EXPECT_EQ(nullptr, context.dispatch.crtDispatch->clEnqueueReleaseDX9ObjectsINTEL);

    EXPECT_EQ(nullptr, context.dispatch.icdDispatch->clGetDeviceIDsFromDX9MediaAdapterKHR);
    EXPECT_EQ(nullptr, context.dispatch.icdDispatch->clCreateFromDX9MediaSurfaceKHR);
    EXPECT_EQ(nullptr, context.dispatch.icdDispatch->clEnqueueAcquireDX9MediaSurfacesKHR);
    EXPECT_EQ(nullptr, context.dispatch.icdDispatch->clEnqueueReleaseDX9MediaSurfacesKHR);

    EXPECT_EQ(nullptr, context.dispatch.icdDispatch->clGetDeviceIDsFromD3D10KHR);
    EXPECT_EQ(nullptr, context.dispatch.icdDispatch->clCreateFromD3D10BufferKHR);
    EXPECT_EQ(nullptr, context.dispatch.icdDispatch->clCreateFromD3D10Texture2DKHR);
    EXPECT_EQ(nullptr, context.dispatch.icdDispatch->clCreateFromD3D10Texture3DKHR);
    EXPECT_EQ(nullptr, context.dispatch.icdDispatch->clEnqueueAcquireD3D10ObjectsKHR);
    EXPECT_EQ(nullptr, context.dispatch.icdDispatch->clEnqueueReleaseD3D10ObjectsKHR);

    EXPECT_EQ(nullptr, context.dispatch.icdDispatch->clGetDeviceIDsFromD3D11KHR);
    EXPECT_EQ(nullptr, context.dispatch.icdDispatch->clCreateFromD3D11BufferKHR);
    EXPECT_EQ(nullptr, context.dispatch.icdDispatch->clCreateFromD3D11Texture2DKHR);
    EXPECT_EQ(nullptr, context.dispatch.icdDispatch->clCreateFromD3D11Texture3DKHR);
    EXPECT_EQ(nullptr, context.dispatch.icdDispatch->clEnqueueAcquireD3D11ObjectsKHR);
    EXPECT_EQ(nullptr, context.dispatch.icdDispatch->clEnqueueReleaseD3D11ObjectsKHR);
}

TEST(DeviceOsTest, DeviceCreationFailMidThreadPreemption) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread));
    auto pDevice = MockDevice::createWithNewExecutionEnvironment<FailDeviceAfterOne>(*platformDevices);

    EXPECT_THAT(pDevice, nullptr);
}
} // namespace NEO
