/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/helpers/get_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/unit_test/helpers/gtest_helpers.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "gtest/gtest.h"

using namespace ::testing;

namespace NEO {

TEST(DeviceOsTest, GivenDefaultClDeviceWhenCheckingForOsSpecificExtensionsThenCorrectExtensionsAreSet) {
    auto hwInfo = defaultHwInfo.get();
    auto pDevice = MockDevice::createWithNewExecutionEnvironment<Device>(hwInfo);
    auto pClDevice = new ClDevice{*pDevice, platform()};

    std::string extensionString(pClDevice->getDeviceInfo().deviceExtensions);

    EXPECT_FALSE(hasSubstr(extensionString, std::string("cl_intel_va_api_media_sharing ")));
    EXPECT_TRUE(hasSubstr(extensionString, std::string("cl_intel_dx9_media_sharing ")));
    EXPECT_TRUE(hasSubstr(extensionString, std::string("cl_khr_dx9_media_sharing ")));
    EXPECT_TRUE(hasSubstr(extensionString, std::string("cl_khr_d3d10_sharing ")));
    EXPECT_TRUE(hasSubstr(extensionString, std::string("cl_khr_d3d11_sharing ")));
    EXPECT_TRUE(hasSubstr(extensionString, std::string("cl_intel_d3d11_nv12_media_sharing ")));
    EXPECT_TRUE(hasSubstr(extensionString, std::string("cl_intel_simultaneous_sharing ")));

    delete pClDevice;
}

TEST(DeviceOsTest, WhenCreatingDeviceThenSimultaneousInteropsIsSupported) {
    auto pDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    std::vector<unsigned int> expected = {CL_GL_CONTEXT_KHR,
                                          CL_WGL_HDC_KHR,
                                          CL_CONTEXT_ADAPTER_D3D9_KHR,
                                          CL_CONTEXT_D3D9_DEVICE_INTEL,
                                          CL_CONTEXT_ADAPTER_D3D9EX_KHR,
                                          CL_CONTEXT_D3D9EX_DEVICE_INTEL,
                                          CL_CONTEXT_ADAPTER_DXVA_KHR,
                                          CL_CONTEXT_DXVA_DEVICE_INTEL,
                                          CL_CONTEXT_D3D10_DEVICE_KHR,
                                          CL_CONTEXT_D3D11_DEVICE_KHR,
                                          0};

    EXPECT_TRUE(pDevice->simultaneousInterops == expected);
}

TEST(DeviceOsTest, GivenFailedDeviceWhenCreatingWithNewExecutionEnvironmentThenNullIsReturned) {
    auto hwInfo = defaultHwInfo.get();
    auto pDevice = MockDevice::createWithNewExecutionEnvironment<FailDevice>(hwInfo);

    EXPECT_EQ(nullptr, pDevice);
}

TEST(DeviceOsTest, GivenMidThreadPreemptionAndFailedDeviceWhenCreatingDeviceThenNullIsReturned) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread));
    auto pDevice = MockDevice::createWithNewExecutionEnvironment<FailDeviceAfterOne>(defaultHwInfo.get());

    EXPECT_EQ(nullptr, pDevice);
}
} // namespace NEO
