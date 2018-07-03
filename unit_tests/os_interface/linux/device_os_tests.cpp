/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/device/device.h"
#include "runtime/helpers/options.h"
#include "runtime/api/api.h"
#include "runtime/platform/platform.h"
#include "runtime/helpers/get_info.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_buffer.h"

using namespace ::testing;

namespace OCLRT {
TEST(DeviceOsTest, osSpecificExtensions) {
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

TEST(DeviceOsTest, supportedSimultaneousInterops) {
    auto pDevice = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(*platformDevices));

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
    auto pDevice = MockDevice::createWithNewExecutionEnvironment<FailDeviceAfterOne>(nullptr);

    EXPECT_THAT(pDevice, nullptr);
}
} // namespace OCLRT
