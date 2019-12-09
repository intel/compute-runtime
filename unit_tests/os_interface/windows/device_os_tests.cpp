/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/options.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/device/device.h"
#include "runtime/helpers/get_info.h"
#include "unit_tests/mocks/mock_device.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace ::testing;

namespace NEO {
TEST(DeviceOsTest, osSpecificExtensions) {
    auto hwInfo = *platformDevices;
    auto pDevice = MockDevice::createWithNewExecutionEnvironment<Device>(hwInfo);

    std::string extensionString(pDevice->getDeviceInfo().deviceExtensions);

    EXPECT_THAT(extensionString, Not(HasSubstr(std::string("cl_intel_va_api_media_sharing "))));
    EXPECT_THAT(extensionString, HasSubstr(std::string("cl_intel_dx9_media_sharing ")));
    EXPECT_THAT(extensionString, HasSubstr(std::string("cl_khr_dx9_media_sharing ")));
    EXPECT_THAT(extensionString, HasSubstr(std::string("cl_khr_d3d10_sharing ")));
    EXPECT_THAT(extensionString, HasSubstr(std::string("cl_khr_d3d11_sharing ")));
    EXPECT_THAT(extensionString, HasSubstr(std::string("cl_intel_d3d11_nv12_media_sharing ")));
    EXPECT_THAT(extensionString, HasSubstr(std::string("cl_intel_simultaneous_sharing ")));

    delete pDevice;
}

TEST(DeviceOsTest, supportedSimultaneousInterops) {
    auto pDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));

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

TEST(DeviceOsTest, DeviceCreationFail) {
    auto hwInfo = *platformDevices;
    auto pDevice = MockDevice::createWithNewExecutionEnvironment<FailDevice>(hwInfo);

    EXPECT_THAT(pDevice, nullptr);
}

TEST(DeviceOsTest, DeviceCreationFailMidThreadPreemption) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread));
    auto pDevice = MockDevice::createWithNewExecutionEnvironment<FailDeviceAfterOne>(*platformDevices);

    EXPECT_THAT(pDevice, nullptr);
}
} // namespace NEO
