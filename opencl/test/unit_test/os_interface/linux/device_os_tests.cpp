/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/source/api/api.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "gtest/gtest.h"

using namespace ::testing;

namespace NEO {

TEST(DeviceOsTest, GivenDefaultClDeviceWhenCheckingForOsSpecificExtensionsThenCorrectExtensionsAreSet) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    DebugManagerStateRestore stateRestore;
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;

    auto hwInfo = defaultHwInfo.get();
    auto pDevice = MockDevice::createWithNewExecutionEnvironment<Device>(hwInfo);
    DeviceFactory::prepareDeviceEnvironments(*pDevice->getExecutionEnvironment());
    auto pClDevice = new ClDevice{*pDevice, platform()};
    std::string extensionString(pClDevice->getDeviceInfo().deviceExtensions);

    EXPECT_FALSE(hasSubstr(extensionString, std::string("cl_intel_dx9_media_sharing ")));
    EXPECT_FALSE(hasSubstr(extensionString, std::string("cl_khr_dx9_media_sharing ")));
    EXPECT_FALSE(hasSubstr(extensionString, std::string("cl_khr_d3d10_sharing ")));
    EXPECT_FALSE(hasSubstr(extensionString, std::string("cl_khr_d3d11_sharing ")));
    EXPECT_FALSE(hasSubstr(extensionString, std::string("cl_intel_d3d11_nv12_media_sharing ")));
    EXPECT_FALSE(hasSubstr(extensionString, std::string("cl_intel_simultaneous_sharing ")));

    delete pClDevice;
}

TEST(DeviceOsTest, GivenDefaultClDeviceWhenCheckingForOsSpecificExtensionsThenDeviceInfoReportsSupportOfExternalMemorySharing) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;

    auto hwInfo = defaultHwInfo.get();
    auto pDevice = MockDevice::createWithNewExecutionEnvironment<Device>(hwInfo);
    DeviceFactory::prepareDeviceEnvironments(*pDevice->getExecutionEnvironment());
    auto pClDevice = new ClDevice{*pDevice, platform()};

    EXPECT_EQ(pClDevice->getDeviceInfo().externalMemorySharing, static_cast<cl_uint>(CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR));

    delete pClDevice;
}

TEST(DeviceOsTest, WhenDeviceIsCreatedThenSimultaneousInteropsIsSupported) {
    auto pDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    std::vector<unsigned int> expected = {0};

    EXPECT_TRUE(pDevice->simultaneousInterops == expected);
}

TEST(ApiOsTest, GivenUnsupportedApiTokensWhenGettingInfoThenInvalidValueErrorIsReturned) {
    MockContext context;
    MockBuffer buffer;

    cl_bool boolVal;
    size_t size;
    auto retVal = context.getInfo(CL_CONTEXT_D3D10_PREFER_SHARED_RESOURCES_KHR, sizeof(cl_bool), &boolVal, &size);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    void *paramVal = nullptr;
    buffer.context = &context;
    retVal = buffer.getMemObjectInfo(CL_MEM_D3D10_RESOURCE_KHR, sizeof(void *), paramVal, &size);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    buffer.context = nullptr;
}

TEST(ApiOsTest, GivenUnsupportedApiWhenGettingDispatchThenNullIsReturned) {
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
} // namespace NEO
