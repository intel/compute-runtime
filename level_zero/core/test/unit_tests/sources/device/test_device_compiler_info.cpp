/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/compiler_interface/spirv_extensions_yaml_igc_sample.h"
#include "shared/test/common/mocks/mock_compiler_interface.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include <algorithm>
#include <array>

namespace L0::ult {

struct DeviceCompilerInfoTest : ::testing::Test, DeviceFixture {
    void SetUp() override {
        DeviceFixture::setUp();
        auto compiler = std::make_unique<NEO::MockCompilerInterface>();
        compilerInterface = compiler.get();
        compiler->spirvExtensionsYAMLOverride = NEO::spirvExtensionsYamlIgcSample;
        neoDevice->getRootDeviceEnvironmentRef().compilerInterface = std::move(compiler);
    }
    void TearDown() override {
        DeviceFixture::tearDown();
    }
    ze_result_t query(ze_device_compiler_info_t paramName, size_t *size, void *data) {
        return zeDeviceGetCompilerInfo(device->toHandle(), paramName, nullptr, size, data);
    }
    NEO::MockCompilerInterface *compilerInterface = nullptr;
    DebugManagerStateRestore restorer;
};

TEST_F(DeviceCompilerInfoTest, givenInvalidArgumentsWhenQueryingCompilerInfoThenCompilerIsNotQueried) {
    size_t size = 13;
    for (auto invalid : {0u, 5u, static_cast<uint32_t>(ZE_DEVICE_COMPILER_INFO_FORCE_UINT32)}) {
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ENUMERATION, query(static_cast<ze_device_compiler_info_t>(invalid), &size, nullptr));
    }
    for (auto unsupported : {ZE_DEVICE_COMPILER_INFO_COMPILER_OPTIONS, ZE_DEVICE_COMPILER_INFO_DRIVER_OPTIONS}) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION, query(unsupported, &size, nullptr));
    }
    ze_base_desc_t next = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zeDeviceGetCompilerInfo(device->toHandle(), ZE_DEVICE_COMPILER_INFO_SPIRV_CAPABILITIES, &next, &size, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_POINTER, query(ZE_DEVICE_COMPILER_INFO_SPIRV_CAPABILITIES, nullptr, nullptr));
    EXPECT_EQ(13u, size);
    EXPECT_EQ(0u, compilerInterface->getSpirvExtensionsYAMLCalled);
}

TEST_F(DeviceCompilerInfoTest, givenIgcDataWhenQueryingCapabilitiesThenSharedCapabilitiesAreReturned) {
    size_t size = 0;
    ASSERT_EQ(ZE_RESULT_SUCCESS, query(ZE_DEVICE_COMPILER_INFO_SPIRV_CAPABILITIES, &size, nullptr));
    ASSERT_GT(size, 0u);
    ASSERT_EQ(0u, size % sizeof(uint32_t));
    std::vector<uint32_t> capabilities(size / sizeof(uint32_t));
    ASSERT_EQ(ZE_RESULT_SUCCESS, query(ZE_DEVICE_COMPILER_INFO_SPIRV_CAPABILITIES, &size, capabilities.data()));
    EXPECT_EQ(neoDevice->getDeviceInfo().spirvCapabilities, capabilities);
}

TEST_F(DeviceCompilerInfoTest, givenIgcDataWhenQueryingExtensionsThenNamesAreNullTerminatedInFixedSizeSlots) {
    size_t size = 0;
    ASSERT_EQ(ZE_RESULT_SUCCESS, query(ZE_DEVICE_COMPILER_INFO_SPIRV_EXTENSIONS, &size, nullptr));
    ASSERT_EQ(NEO::spirvExtensionsYamlIgcSampleExtensionCount * ZE_MAX_EXTENSION_NAME, size);
    std::vector<char> extensions(size, '\x7f');
    ASSERT_EQ(ZE_RESULT_SUCCESS, query(ZE_DEVICE_COMPILER_INFO_SPIRV_EXTENSIONS, &size, extensions.data()));
    const auto &expected = neoDevice->getDeviceInfo().spirvExtensions;
    for (size_t i = 0; i < expected.size(); ++i) {
        const char *name = extensions.data() + i * ZE_MAX_EXTENSION_NAME;
        EXPECT_EQ(expected[i], name);
        EXPECT_TRUE(std::all_of(name + expected[i].size(), name + ZE_MAX_EXTENSION_NAME, [](char value) { return value == 0; }));
    }
}

TEST_F(DeviceCompilerInfoTest, givenNullOutputBufferAndOversizedInputSizeWhenQueryingCompilerInfoThenRequiredSizeIsReturned) {
    for (auto param : {ZE_DEVICE_COMPILER_INFO_SPIRV_CAPABILITIES, ZE_DEVICE_COMPILER_INFO_SPIRV_EXTENSIONS}) {
        size_t requiredSize = 0;
        ASSERT_EQ(ZE_RESULT_SUCCESS, query(param, &requiredSize, nullptr));
        size_t inputSize = requiredSize + 1;
        ASSERT_EQ(ZE_RESULT_SUCCESS, query(param, &inputSize, nullptr));
        EXPECT_EQ(requiredSize, inputSize);
    }
}

TEST_F(DeviceCompilerInfoTest, givenZeroOrUndersizedBufferWhenQueryingCompilerInfoThenRequiredSizeIsReturnedWithoutWriting) {
    for (auto param : {ZE_DEVICE_COMPILER_INFO_SPIRV_CAPABILITIES, ZE_DEVICE_COMPILER_INFO_SPIRV_EXTENSIONS}) {
        size_t requiredSize = 0;
        ASSERT_EQ(ZE_RESULT_SUCCESS, query(param, &requiredSize, nullptr));
        ASSERT_GT(requiredSize, 1u);
        for (size_t inputSize : {size_t{0}, requiredSize - 1}) {
            std::vector<uint8_t> buffer(requiredSize, 0xab);
            EXPECT_EQ(ZE_RESULT_SUCCESS, query(param, &inputSize, buffer.data()));
            EXPECT_EQ(requiredSize, inputSize);
            EXPECT_TRUE(std::all_of(buffer.begin(), buffer.end(), [](uint8_t value) { return value == 0xab; }));
        }
    }
}

TEST_F(DeviceCompilerInfoTest, givenOversizedBufferWhenQueryingCompilerInfoThenOnlyResultBytesAreWritten) {
    for (auto param : {ZE_DEVICE_COMPILER_INFO_SPIRV_CAPABILITIES, ZE_DEVICE_COMPILER_INFO_SPIRV_EXTENSIONS}) {
        size_t requiredSize = 0;
        ASSERT_EQ(ZE_RESULT_SUCCESS, query(param, &requiredSize, nullptr));
        std::vector<uint8_t> buffer(requiredSize + 8, 0xab);
        size_t size = buffer.size();
        EXPECT_EQ(ZE_RESULT_SUCCESS, query(param, &size, buffer.data()));
        EXPECT_EQ(requiredSize, size);
        EXPECT_TRUE(std::all_of(buffer.begin() + requiredSize, buffer.end(), [](uint8_t value) { return value == 0xab; }));
    }
}

TEST_F(DeviceCompilerInfoTest, givenEmptyCompilerDataWhenQueryingCompilerInfoThenBaseCapabilitiesAndNoExtensionsAreReturned) {
    compilerInterface->spirvExtensionsYAMLOverride = "";
    size_t size = 1;
    char output = 'x';
    EXPECT_EQ(ZE_RESULT_SUCCESS, query(ZE_DEVICE_COMPILER_INFO_SPIRV_EXTENSIONS, &size, &output));
    EXPECT_EQ(0u, size);
    EXPECT_EQ('x', output);
    ASSERT_EQ(ZE_RESULT_SUCCESS, query(ZE_DEVICE_COMPILER_INFO_SPIRV_CAPABILITIES, &size, nullptr));
    std::vector<uint32_t> capabilities(size / sizeof(uint32_t));
    ASSERT_EQ(ZE_RESULT_SUCCESS, query(ZE_DEVICE_COMPILER_INFO_SPIRV_CAPABILITIES, &size, capabilities.data()));
    EXPECT_EQ(neoDevice->getSpirvBaseCapabilities(), capabilities);
}

TEST_F(DeviceCompilerInfoTest, givenLongestRepresentableExtensionNameWhenQueryingExtensionsThenTerminatorFits) {
    const std::string name(ZE_MAX_EXTENSION_NAME - 1, 'a');
    compilerInterface->spirvExtensionsYAMLOverride = "- name: " + name + "\n";
    std::array<char, ZE_MAX_EXTENSION_NAME> output;
    output.fill('x');
    size_t size = output.size();
    EXPECT_EQ(ZE_RESULT_SUCCESS, query(ZE_DEVICE_COMPILER_INFO_SPIRV_EXTENSIONS, &size, output.data()));
    EXPECT_EQ(name, output.data());
    EXPECT_EQ('\0', output.back());
}

TEST_F(DeviceCompilerInfoTest, givenUnrepresentableExtensionNameWhenQueryingExtensionsThenErrorIsReturnedWithoutTruncation) {
    compilerInterface->spirvExtensionsYAMLOverride = "- name: " + std::string(ZE_MAX_EXTENSION_NAME, 'a') + "\n";
    std::array<char, ZE_MAX_EXTENSION_NAME> output;
    output.fill('x');
    size_t size = output.size();
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, query(ZE_DEVICE_COMPILER_INFO_SPIRV_EXTENSIONS, &size, output.data()));
    EXPECT_TRUE(std::all_of(output.begin(), output.end(), [](char value) { return value == 'x'; }));
}

} // namespace L0::ult
