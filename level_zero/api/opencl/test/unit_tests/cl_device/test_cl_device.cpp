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

#include "spirv/unified1/spirv.hpp"

#include <algorithm>
#include <string>
#include <unordered_set>

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

TEST_F(ClDeviceTests, givenSubgroupRotateSupportWhenQueryingDeviceInfoThenInteropDriverReportsOpenClAndSpirvSupport) {
    auto &devices = platform->getDevices();
    ASSERT_FALSE(devices.empty());
    auto *clDevice = devices[0].get();

    size_t deviceExtensionsSize = 0;
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_EXTENSIONS, 0, nullptr, &deviceExtensionsSize));
    std::vector<char> deviceExtensions(deviceExtensionsSize);
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_EXTENSIONS, deviceExtensionsSize, deviceExtensions.data(), nullptr));
    EXPECT_NE(std::string::npos, std::string(deviceExtensions.data()).find("cl_khr_subgroup_rotate"));

    size_t spirvExtensionsSize = 0;
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_SPIRV_EXTENSIONS_KHR, 0, nullptr, &spirvExtensionsSize));
    std::vector<const char *> spirvExtensions(spirvExtensionsSize / sizeof(const char *));
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_SPIRV_EXTENSIONS_KHR, spirvExtensionsSize, spirvExtensions.data(), nullptr));
    EXPECT_TRUE(std::any_of(spirvExtensions.begin(), spirvExtensions.end(), [](const char *extension) {
        return std::string("SPV_KHR_subgroup_rotate") == extension;
    }));

    size_t spirvCapabilitiesSize = 0;
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_SPIRV_CAPABILITIES_KHR, 0, nullptr, &spirvCapabilitiesSize));
    std::vector<cl_uint> spirvCapabilities(spirvCapabilitiesSize / sizeof(cl_uint));
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_SPIRV_CAPABILITIES_KHR, spirvCapabilitiesSize, spirvCapabilities.data(), nullptr));
    EXPECT_NE(spirvCapabilities.end(),
              std::find(spirvCapabilities.begin(), spirvCapabilities.end(),
                        static_cast<cl_uint>(spv::CapabilityGroupNonUniformRotateKHR)));
}

TEST_F(ClDeviceTests, GivenIgcProvidesSpirvYamlWhenQueryingSpirvInfoThenInteropDriverReportsIgcData) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableSpirvQueriesFromIgc.set(1);
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
    EXPECT_GE(extSize / sizeof(const char *), spirvExtensionsYamlIgcSampleExtensionCount);

    std::vector<const char *> extensions(extSize / sizeof(const char *));
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_SPIRV_EXTENSIONS_KHR, extSize, extensions.data(), nullptr));
    EXPECT_TRUE(std::any_of(extensions.begin(), extensions.end(), [](const char *e) { return std::string("SPV_KHR_shader_clock") == e; }));

    size_t capsSize = 0;
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_SPIRV_CAPABILITIES_KHR, 0, nullptr, &capsSize));
    std::vector<cl_uint> capabilities(capsSize / sizeof(cl_uint));
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_SPIRV_CAPABILITIES_KHR, capsSize, capabilities.data(), nullptr));
    auto hasCapability = [&capabilities](cl_uint cap) {
        return std::find(capabilities.begin(), capabilities.end(), cap) != capabilities.end();
    };
    // The mandatory base capabilities are always reported (not tied to any SPIR-V extension,
    // so IGC does not report them), merged with the IGC set: Addresses=4, Linkage=5, Kernel=6,
    // Vector16=7, Float16Buffer=8, Int64=11, Int16=22, Int8=39.
    for (cl_uint baseCap : {4u, 5u, 6u, 7u, 8u, 11u, 22u, 39u}) {
        EXPECT_TRUE(hasCapability(baseCap)) << "missing required base capability " << baseCap;
    }
    EXPECT_GE(capabilities.size(), spirvExtensionsYamlIgcSampleCapabilityCount);
}

TEST_F(ClDeviceTests, GivenDeviceDependentSpirvCapabilitiesThenIgcPathReportsThemLikeLegacyPath) {
    DebugManagerStateRestore restorer;
    auto readSpirvInfo = [this](bool useIgc) {
        debugManager.flags.EnableSpirvQueriesFromIgc.set(useIgc ? 1 : 0);
        Platform localPlatform(driverHandle->toHandle());
        auto *clDevice = localPlatform.getDevices()[0].get();
        if (useIgc) {
            auto &neoDevice = clDevice->getDevice();
            auto *mockCompilerInterface = new MockCompilerInterface();
            mockCompilerInterface->spirvExtensionsYAMLOverride = std::string(spirvExtensionsYamlIgcSample);
            neoDevice.getExecutionEnvironment()->rootDeviceEnvironments[neoDevice.getRootDeviceIndex()]->compilerInterface.reset(mockCompilerInterface);
        }

        size_t capsSize = 0;
        clDevice->getDeviceInfo(CL_DEVICE_SPIRV_CAPABILITIES_KHR, 0, nullptr, &capsSize);
        std::vector<cl_uint> capabilities(capsSize / sizeof(cl_uint));
        clDevice->getDeviceInfo(CL_DEVICE_SPIRV_CAPABILITIES_KHR, capsSize, capabilities.data(), nullptr);

        size_t extSize = 0;
        clDevice->getDeviceInfo(CL_DEVICE_SPIRV_EXTENSIONS_KHR, 0, nullptr, &extSize);
        std::vector<const char *> extensions(extSize / sizeof(const char *));
        clDevice->getDeviceInfo(CL_DEVICE_SPIRV_EXTENSIONS_KHR, extSize, extensions.data(), nullptr);

        std::unordered_set<std::string> extensionSet;
        for (const auto *e : extensions) {
            extensionSet.emplace(e);
        }
        return std::pair<std::unordered_set<cl_uint>, std::unordered_set<std::string>>{
            std::unordered_set<cl_uint>(capabilities.begin(), capabilities.end()), std::move(extensionSet)};
    };

    const auto [legacyCapabilities, legacyExtensions] = readSpirvInfo(false);
    const auto [igcCapabilities, igcExtensions] = readSpirvInfo(true);

    for (auto deviceDependentCap : {spv::CapabilityAtomicFloat32AddEXT, spv::CapabilityAtomicFloat32MinMaxEXT,
                                    spv::CapabilityAtomicFloat16AddEXT, spv::CapabilityAtomicFloat16MinMaxEXT,
                                    spv::CapabilityAtomicFloat64AddEXT, spv::CapabilityAtomicFloat64MinMaxEXT,
                                    spv::CapabilityDotProductInput4x8Bit,
                                    // Capabilities derived from the supported OpenCL extensions, IGC may not report
                                    // the associated SPIR-V extension, so they must be applied unconditionally
                                    spv::CapabilityExpectAssumeKHR, spv::CapabilityBitInstructions,
                                    spv::CapabilityDotProduct, spv::CapabilityDotProductInput4x8BitPacked,
                                    spv::CapabilityShaderClockKHR, spv::CapabilityGroupNonUniformRotateKHR,
                                    spv::CapabilityGroupUniformArithmeticKHR, spv::CapabilityBFloat16ConversionINTEL,
                                    spv::CapabilitySubgroupAvcMotionEstimationINTEL, spv::CapabilitySubgroupAvcMotionEstimationChromaINTEL,
                                    spv::CapabilitySubgroupAvcMotionEstimationIntraINTEL, spv::CapabilitySubgroupImageMediaBlockIOINTEL,
                                    spv::CapabilitySubgroupBufferBlockIOINTEL, spv::CapabilitySubgroupImageBlockIOINTEL,
                                    spv::CapabilitySubgroupShuffleINTEL, spv::CapabilitySplitBarrierINTEL,
                                    spv::CapabilitySubgroupBufferPrefetchINTEL}) {
        const auto cap = static_cast<cl_uint>(deviceDependentCap);
        if (legacyCapabilities.contains(cap)) {
            EXPECT_EQ(1u, igcCapabilities.count(cap)) << "device-dependent capability " << cap << " missing from the IGC path";
        }
    }

    for (const auto *deviceDependentExtension : {"SPV_EXT_shader_atomic_float_add", "SPV_EXT_shader_atomic_float16_add",
                                                 "SPV_EXT_shader_atomic_float_min_max",
                                                 "SPV_KHR_expect_assume", "SPV_KHR_bit_instructions",
                                                 "SPV_KHR_integer_dot_product", "SPV_KHR_shader_clock",
                                                 "SPV_KHR_linkonce_odr", "SPV_KHR_no_integer_wrap_decoration",
                                                 "SPV_KHR_subgroup_rotate", "SPV_KHR_uniform_group_instructions",
                                                 "SPV_INTEL_bfloat16_conversion", "SPV_INTEL_device_side_avc_motion_estimation",
                                                 "SPV_INTEL_media_block_io", "SPV_INTEL_subgroups",
                                                 "SPV_INTEL_split_barrier", "SPV_INTEL_subgroup_buffer_prefetch"}) {
        if (legacyExtensions.contains(deviceDependentExtension)) {
            EXPECT_EQ(1u, igcExtensions.count(deviceDependentExtension)) << "device-dependent extension " << deviceDependentExtension << " missing from the IGC path";
        }
    }
}

TEST_F(ClDeviceTests, GivenExtensionReportedByBothStaticDerivationAndIgcWhenQueryingSpirvExtensionsThenItIsNotDuplicated) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableSpirvQueriesFromIgc.set(1);
    auto &devices = platform->getDevices();
    ASSERT_FALSE(devices.empty());
    auto *clDevice = devices[0].get();
    auto &neoDevice = clDevice->getDevice();

    auto *mockCompilerInterface = new MockCompilerInterface();
    mockCompilerInterface->spirvExtensionsYAMLOverride = std::string(spirvExtensionsYamlIgcSample);
    neoDevice.getExecutionEnvironment()->rootDeviceEnvironments[neoDevice.getRootDeviceIndex()]->compilerInterface.reset(mockCompilerInterface);

    size_t extSize = 0;
    ASSERT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_SPIRV_EXTENSIONS_KHR, 0, nullptr, &extSize));
    std::vector<const char *> extensions(extSize / sizeof(const char *));
    ASSERT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_SPIRV_EXTENSIONS_KHR, extSize, extensions.data(), nullptr));

    auto occurrences = [&extensions](const char *name) {
        return std::count_if(extensions.begin(), extensions.end(), [name](const char *e) { return std::string(name) == e; });
    };

    EXPECT_EQ(1, occurrences("SPV_INTEL_subgroups"));
    EXPECT_GE(1, occurrences("SPV_KHR_shader_clock"));
    EXPECT_GE(1, occurrences("SPV_KHR_bit_instructions"));
}

} // namespace ult
} // namespace LEO
} // namespace NEO
