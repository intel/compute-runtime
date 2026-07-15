/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/spirv_capabilities_parser.h"
#include "shared/test/common/compiler_interface/spirv_extensions_yaml_igc_sample.h"
#include "shared/test/common/test_macros/test.h"

#include <algorithm>

using namespace NEO;

// Test parsing YAML with numeric IDs (new IGC format)
TEST(SpirvCapabilitiesParserTest, GivenYamlWithNumericIdsThenParsesCorrectly) {
    // YAML format that IGC now generates (id before name)
    std::string yamlText = R"(
spirv_extensions:
  - name: SPV_INTEL_subgroups
    url: "https://github.com/KhronosGroup/SPIRV-Registry/blob/main/extensions/INTEL/SPV_INTEL_subgroups.asciidoc"
    supported_capabilities:
      - id: 5568
        name: SubgroupShuffleINTEL
      - id: 5569
        name: SubgroupBufferBlockIOINTEL
      - id: 5570
        name: SubgroupImageBlockIOINTEL
  - name: SPV_INTEL_cache_controls
    url: "https://github.com/KhronosGroup/SPIRV-Registry/blob/main/extensions/INTEL/SPV_INTEL_cache_controls.asciidoc"
    supported_capabilities:
      - id: 6441
        name: CacheControlsINTEL
)";

    std::vector<SpirvExtensionInfo> extensions;
    std::string errReason, warning;

    bool result = SpirvCapabilitiesParser::parseSpirvExtensionsYAML(yamlText, extensions, errReason, warning);

    EXPECT_TRUE(result) << "Parse failed: " << errReason;
    EXPECT_EQ(2u, extensions.size());

    // Verify first extension
    ASSERT_GE(extensions.size(), 1u);
    EXPECT_EQ("SPV_INTEL_subgroups", extensions[0].name);
    EXPECT_EQ(3u, extensions[0].supportedCapabilities.size());

    // Verify the parser extracted the numeric id + name from the YAML.
    EXPECT_EQ(5568u, extensions[0].supportedCapabilities[0].id);
    EXPECT_EQ("SubgroupShuffleINTEL", extensions[0].supportedCapabilities[0].name);

    EXPECT_EQ(5569u, extensions[0].supportedCapabilities[1].id);
    EXPECT_EQ("SubgroupBufferBlockIOINTEL", extensions[0].supportedCapabilities[1].name);

    EXPECT_EQ(5570u, extensions[0].supportedCapabilities[2].id);
    EXPECT_EQ("SubgroupImageBlockIOINTEL", extensions[0].supportedCapabilities[2].name);

    // Verify second extension
    ASSERT_GE(extensions.size(), 2u);
    EXPECT_EQ("SPV_INTEL_cache_controls", extensions[1].name);
    EXPECT_EQ(1u, extensions[1].supportedCapabilities.size());
    EXPECT_EQ(6441u, extensions[1].supportedCapabilities[0].id);
    EXPECT_EQ("CacheControlsINTEL", extensions[1].supportedCapabilities[0].name);
}

// Test order-agnostic parsing (name before id should also work)
TEST(SpirvCapabilitiesParserTest, GivenYamlWithNameBeforeIdThenParsesCorrectly) {
    std::string yamlText = R"(
spirv_extensions:
  - name: SPV_KHR_expect_assume
    url: "https://github.com/KhronosGroup/SPIRV-Registry/blob/main/extensions/KHR/SPV_KHR_expect_assume.asciidoc"
    supported_capabilities:
      - name: ExpectAssumeKHR
        id: 5629
)";

    std::vector<SpirvExtensionInfo> extensions;
    std::string errReason, warning;

    bool result = SpirvCapabilitiesParser::parseSpirvExtensionsYAML(yamlText, extensions, errReason, warning);

    EXPECT_TRUE(result);
    EXPECT_EQ(1u, extensions.size());
    EXPECT_EQ("SPV_KHR_expect_assume", extensions[0].name);
    EXPECT_EQ(1u, extensions[0].supportedCapabilities.size());
    EXPECT_EQ(5629u, extensions[0].supportedCapabilities[0].id);
    EXPECT_EQ("ExpectAssumeKHR", extensions[0].supportedCapabilities[0].name);
}

// Test with new capabilities with manual ID overrides (from IGC patch)
TEST(SpirvCapabilitiesParserTest, GivenYamlWithManualIdOverridesThenParsesCorrectly) {
    std::string yamlText = R"(
spirv_extensions:
  - name: SPV_INTEL_bfloat16_arithmetic
    url: "https://github.com/intel/llvm/blob/sycl/sycl/doc/design/spirv-extensions/SPV_INTEL_bfloat16_arithmetic.asciidoc"
    supported_capabilities:
      - id: 6226
        name: BFloat16ArithmeticINTEL
  - name: SPV_INTEL_16bit_atomics
    url: "https://github.com/intel/llvm/pull/20009"
    supported_capabilities:
      - id: 6260
        name: AtomicInt16CompareExchangeINTEL
      - id: 6262
        name: AtomicBFloat16LoadStoreINTEL
      - id: 6255
        name: AtomicBFloat16AddINTEL
      - id: 6256
        name: AtomicBFloat16MinMaxINTEL
)";

    std::vector<SpirvExtensionInfo> extensions;
    std::string errReason, warning;

    bool result = SpirvCapabilitiesParser::parseSpirvExtensionsYAML(yamlText, extensions, errReason, warning);

    EXPECT_TRUE(result) << "Parse failed: " << errReason;
    EXPECT_EQ(2u, extensions.size());

    // BFloat16 extension
    EXPECT_EQ("SPV_INTEL_bfloat16_arithmetic", extensions[0].name);
    EXPECT_EQ(1u, extensions[0].supportedCapabilities.size());
    EXPECT_EQ(6226u, extensions[0].supportedCapabilities[0].id);
    EXPECT_EQ("BFloat16ArithmeticINTEL", extensions[0].supportedCapabilities[0].name);

    // 16bit atomics extension
    EXPECT_EQ("SPV_INTEL_16bit_atomics", extensions[1].name);
    EXPECT_EQ(4u, extensions[1].supportedCapabilities.size());
    EXPECT_EQ(6260u, extensions[1].supportedCapabilities[0].id);
    EXPECT_EQ(6262u, extensions[1].supportedCapabilities[1].id);
    EXPECT_EQ(6255u, extensions[1].supportedCapabilities[2].id);
    EXPECT_EQ(6256u, extensions[1].supportedCapabilities[3].id);
}

// Test the real format emitted by IGC: a bare top-level sequence (no
// 'spirv_extensions:' wrapper) with 'spec_url' instead of 'url'. This is what
// GetCompilerSupportedSPIRVExtensionsYAML() actually returns.
TEST(SpirvCapabilitiesParserTest, GivenBareSequenceWithSpecUrlThenParsesCorrectly) {
    std::string yamlText = R"(---
- name:            SPV_EXT_optnone
  spec_url:        'https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_optnone.html'
  supported_capabilities:
    - id:              6094
      name:            OptNoneEXT
- name:            SPV_INTEL_cache_controls
  spec_url:        'https://github.com/KhronosGroup/SPIRV-Registry/blob/main/extensions/INTEL/SPV_INTEL_cache_controls.asciidoc'
  supported_capabilities:
    - id:              6441
      name:            CacheControlsINTEL
)";

    std::vector<SpirvExtensionInfo> extensions;
    std::string errReason, warning;

    bool result = SpirvCapabilitiesParser::parseSpirvExtensionsYAML(yamlText, extensions, errReason, warning);

    EXPECT_TRUE(result) << "Parse failed: " << errReason;
    ASSERT_EQ(2u, extensions.size());

    EXPECT_EQ("SPV_EXT_optnone", extensions[0].name);
    ASSERT_EQ(1u, extensions[0].supportedCapabilities.size());
    EXPECT_EQ(6094u, extensions[0].supportedCapabilities[0].id);
    EXPECT_EQ("OptNoneEXT", extensions[0].supportedCapabilities[0].name);

    EXPECT_EQ("SPV_INTEL_cache_controls", extensions[1].name);
    ASSERT_EQ(1u, extensions[1].supportedCapabilities.size());
    EXPECT_EQ(6441u, extensions[1].supportedCapabilities[0].id);
    EXPECT_EQ("CacheControlsINTEL", extensions[1].supportedCapabilities[0].name);
}

TEST(SpirvCapabilitiesParserTest, GivenRealIgcSampleThenParsesAllExtensionsAndCapabilities) {
    std::vector<SpirvExtensionInfo> extensions;
    std::string errReason, warning;

    bool result = SpirvCapabilitiesParser::parseSpirvExtensionsYAML(spirvExtensionsYamlIgcSample, extensions, errReason, warning);

    EXPECT_TRUE(result) << "Parse failed: " << errReason;
    ASSERT_EQ(spirvExtensionsYamlIgcSampleExtensionCount, extensions.size());

    size_t totalCaps = 0;
    for (const auto &ext : extensions) {
        totalCaps += ext.supportedCapabilities.size();
    }
    EXPECT_EQ(spirvExtensionsYamlIgcSampleCapabilityCount, totalCaps);

    // Spot-check the first and last extension and a multi-capability one.
    EXPECT_EQ("SPV_EXT_optnone", extensions.front().name);
    ASSERT_EQ(1u, extensions.front().supportedCapabilities.size());
    EXPECT_EQ(6094u, extensions.front().supportedCapabilities[0].id);
    EXPECT_EQ("OptNoneEXT", extensions.front().supportedCapabilities[0].name);

    EXPECT_EQ("SPV_INTEL_subgroups", extensions[8].name);
    EXPECT_EQ(3u, extensions[8].supportedCapabilities.size());

    EXPECT_EQ("SPV_KHR_shader_clock", extensions.back().name);
    ASSERT_EQ(1u, extensions.back().supportedCapabilities.size());
    EXPECT_EQ(5055u, extensions.back().supportedCapabilities[0].id);

    // An extension advertised with no capabilities (empty sequence) must be
    // kept with an empty capability list, not dropped.
    auto noWrapIt = std::find_if(extensions.begin(), extensions.end(), [](const SpirvExtensionInfo &e) {
        return e.name == "SPV_KHR_no_integer_wrap_decoration";
    });
    ASSERT_NE(extensions.end(), noWrapIt);
    EXPECT_TRUE(noWrapIt->supportedCapabilities.empty());
}

// Test validation - capability without ID should be rejected
TEST(SpirvCapabilitiesParserTest, GivenCapabilityWithoutIdThenIsSkipped) {
    std::string yamlText = R"(
spirv_extensions:
  - name: TestExtension
    url: "https://example.com"
    supported_capabilities:
      - name: OnlyNameNoId
      - id: 1234
        name: ValidCapability
)";

    std::vector<SpirvExtensionInfo> extensions;
    std::string errReason, warning;

    bool result = SpirvCapabilitiesParser::parseSpirvExtensionsYAML(yamlText, extensions, errReason, warning);

    EXPECT_TRUE(result);
    EXPECT_EQ(1u, extensions.size());
    // Capability without ID (capInfo.id == 0) is rejected by validation
    // Verify that only valid capability was accepted
    EXPECT_EQ(1u, extensions[0].supportedCapabilities.size());
    EXPECT_EQ(1234u, extensions[0].supportedCapabilities[0].id);
    EXPECT_EQ("ValidCapability", extensions[0].supportedCapabilities[0].name);
}

// Test empty YAML
TEST(SpirvCapabilitiesParserTest, GivenEmptyYamlThenReturnsTrue) {
    std::string yamlText = R"(
spirv_extensions:
)";

    std::vector<SpirvExtensionInfo> extensions;
    std::string errReason, warning;

    bool result = SpirvCapabilitiesParser::parseSpirvExtensionsYAML(yamlText, extensions, errReason, warning);

    EXPECT_TRUE(result);
    EXPECT_EQ(0u, extensions.size());
}

// Test invalid YAML
TEST(SpirvCapabilitiesParserTest, GivenInvalidYamlThenReturnsFalse) {
    std::string yamlText = "this is not valid yaml: [[[";

    std::vector<SpirvExtensionInfo> extensions;
    std::string errReason, warning;

    bool result = SpirvCapabilitiesParser::parseSpirvExtensionsYAML(yamlText, extensions, errReason, warning);

    EXPECT_FALSE(result);
    EXPECT_FALSE(errReason.empty());
}
