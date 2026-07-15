/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/compiler_interface/spirv_extensions_yaml_igc_sample.h"
#include "shared/test/common/mocks/mock_compiler_interface.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include <algorithm>
#include <string>

namespace NEO {
namespace LEO {
namespace ult {

using ClDeviceTests = Test<OclFixture>;

TEST_F(ClDeviceTests, givenPlatformWhenGetDevicesThenReturnsNonEmptyList) {
    EXPECT_FALSE(platform->getDevices().empty());
}

TEST_F(ClDeviceTests, givenRootDeviceWhenGetIsSubdeviceThenReturnsFalse) {
    auto &devices = platform->getDevices();
    ASSERT_FALSE(devices.empty());
    EXPECT_FALSE(devices[0]->getIsSubdevice());
}

TEST_F(ClDeviceTests, givenRootDeviceWhenGetL0HandleThenReturnsNonNull) {
    auto &devices = platform->getDevices();
    ASSERT_FALSE(devices.empty());
    EXPECT_NE(nullptr, devices[0]->getL0Handle());
}

TEST_F(ClDeviceTests, givenRootDeviceWhenGetHardwareInfoThenIsAccessible) {
    auto &devices = platform->getDevices();
    ASSERT_FALSE(devices.empty());
    const auto &hwInfo = devices[0]->getHardwareInfo();
    EXPECT_NE(0u, hwInfo.platform.eProductFamily);
}

TEST_F(ClDeviceTests, givenRootDeviceWhenGetSharedDeviceInfoThenHasNonZeroMaxWorkGroupSize) {
    auto &devices = platform->getDevices();
    ASSERT_FALSE(devices.empty());
    const auto &deviceInfo = devices[0]->getSharedDeviceInfo();
    EXPECT_GT(deviceInfo.maxWorkGroupSize, 0u);
}

TEST_F(ClDeviceTests, givenRootDeviceWhenGetPlatformHostTimerResolutionThenReturnsNonNegative) {
    auto &devices = platform->getDevices();
    ASSERT_FALSE(devices.empty());
    EXPECT_GE(devices[0]->getPlatformHostTimerResolution(), 0.0);
}

TEST_F(ClDeviceTests, GivenIgcProvidesSpirvYamlWhenQueryingSpirvInfoThenInteropDriverReportsIgcData) {
    auto &devices = platform->getDevices();
    ASSERT_FALSE(devices.empty());
    auto *clDevice = devices[0].get();
    auto &neoDevice = clDevice->getDevice();

    auto *mockCompilerInterface = new MockCompilerInterface();
    mockCompilerInterface->spirvExtensionsYAMLOverride = std::string(spirvExtensionsYamlIgcSample);
    neoDevice.getExecutionEnvironment()->rootDeviceEnvironments[neoDevice.getRootDeviceIndex()]->compilerInterface.reset(mockCompilerInterface);

    size_t extSize = 0;
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_SPIRV_EXTENSIONS_KHR, 0, nullptr, &extSize));
    EXPECT_EQ(1u, mockCompilerInterface->getSpirvExtensionsYAMLCalled);
    EXPECT_EQ(spirvExtensionsYamlIgcSampleExtensionCount, extSize / sizeof(const char *));

    std::vector<const char *> extensions(extSize / sizeof(const char *));
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_SPIRV_EXTENSIONS_KHR, extSize, extensions.data(), nullptr));
    EXPECT_TRUE(std::any_of(extensions.begin(), extensions.end(), [](const char *e) { return std::string("SPV_KHR_shader_clock") == e; }));

    size_t capsSize = 0;
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_SPIRV_CAPABILITIES_KHR, 0, nullptr, &capsSize));
    EXPECT_EQ(spirvExtensionsYamlIgcSampleCapabilityCount, capsSize / sizeof(cl_uint));
}

} // namespace ult
} // namespace LEO
} // namespace NEO
