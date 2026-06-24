/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_options_parser.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/test.h"
using namespace NEO;

using L1CachePolicy = NEO::Zebin::ZeInfo::Types::L1CachePolicy::L1CachePolicy;

TEST(CompilerOptionsParserL1CachePolicy, GivenMissmatchCheckDisabledThenCheckL1CachePolicyMismatchReturnsFalse) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.recompileKernelsWhenL1PolicyMissmatch = false;
    // L1_CACHE_CONTROL_WB (2) vs driver default WBP (0) would mismatch, but the check is disabled.
    EXPECT_FALSE(checkL1CachePolicyMismatch(L1CachePolicy::L1CachePolicyWriteBack, 0u));
}

TEST(CompilerOptionsParserL1CachePolicy, GivenUnknownBinaryPolicyThenCheckL1CachePolicyMismatchReturnsFalse) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.recompileKernelsWhenL1PolicyMissmatch = true;
    EXPECT_FALSE(checkL1CachePolicyMismatch(L1CachePolicy::L1CachePolicyUnknown, 0u));
    EXPECT_FALSE(checkL1CachePolicyMismatch(L1CachePolicy::L1CachePolicyUnknown, 2u));
}

TEST(CompilerOptionsParserL1CachePolicy, GivenKnownBinaryPolicyThenCheckL1CachePolicyMismatchComparesAgainstDriverDefault) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.recompileKernelsWhenL1PolicyMissmatch = true;

    // Mapping mirrors STATE_BASE_ADDRESS::L1_CACHE_CONTROL: WBP=0, UC=1, WB=2, WT=3, WS=4.
    const std::pair<L1CachePolicy, uint32_t> policyToL1CacheControl[] = {
        {L1CachePolicy::L1CachePolicyWriteBypass, 0u},
        {L1CachePolicy::L1CachePolicyUncached, 1u},
        {L1CachePolicy::L1CachePolicyWriteBack, 2u},
        {L1CachePolicy::L1CachePolicyWriteThrough, 3u},
        {L1CachePolicy::L1CachePolicyWriteStreaming, 4u}};

    for (const auto &[policy, l1CacheControl] : policyToL1CacheControl) {
        // Matches the driver default -> no mismatch.
        EXPECT_FALSE(checkL1CachePolicyMismatch(policy, l1CacheControl));
        // Differs from the driver default -> mismatch.
        EXPECT_TRUE(checkL1CachePolicyMismatch(policy, l1CacheControl + 1u));
    }
}

TEST(CompilerOptionsParserL1CachePolicy, GivenUnmappedBinaryPolicyThenCheckL1CachePolicyMismatchReturnsFalse) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.recompileKernelsWhenL1PolicyMissmatch = true;
    // A non-Unknown value that maps to no L1_CACHE_CONTROL entry must not trigger a rebuild.
    EXPECT_FALSE(checkL1CachePolicyMismatch(L1CachePolicy::L1CachePolicyMax, 0u));
}

TEST(CompilerOptionsParser, GivenClStdOptionWithVariousVersionsWhenRequiresOpenClCFeaturesThenCorrectResult) {
    EXPECT_FALSE(requiresOpenClCFeatures("-cl-std=CL1.2"));
    EXPECT_FALSE(requiresOpenClCFeatures("-cl-std=CL2.0"));
    EXPECT_TRUE(requiresOpenClCFeatures("-cl-std=CL3.0"));
    EXPECT_TRUE(requiresOpenClCFeatures("-cl-std=CL4.0"));
}

TEST(CompilerOptionsParser, GivenClStdOptionWithVariousVersionsWhenRequiresAdditionalExtensionsThenCorrectResult) {
    EXPECT_FALSE(requiresAdditionalExtensions("-cl-std=CL1.2"));
    EXPECT_TRUE(requiresAdditionalExtensions("-cl-std=CL2.0"));
    EXPECT_FALSE(requiresAdditionalExtensions("-cl-std=CL3.0"));
}

TEST(CompilerOptionsParser, GivenExtensionsAndCompileOptionsWhenAppendAdditionalExtensionsThenCorrectlyAppends) {
    std::string extensions = "ext1 ";
    std::string compileOptions = "-cl-std=CL2.0";
    std::string internalOptions = "";
    appendAdditionalExtensions(extensions, compileOptions, internalOptions);
    EXPECT_NE(extensions.find("cl_khr_3d_image_writes"), std::string::npos);

    // Test with fp64-gen-emu
    extensions = "ext1 ";
    internalOptions = "-cl-fp64-gen-emu";
    appendAdditionalExtensions(extensions, compileOptions, internalOptions);
    EXPECT_NE(extensions.find("cl_khr_fp64"), std::string::npos);
    EXPECT_NE(extensions.find("__opencl_c_fp64"), std::string::npos);
}

TEST(CompilerOptionsParser, GivenExtensionsWithMsaaSharingWhenRemoveNotSupportedExtensionsThenMsaaSharingIsRemovedForOclVersionBelow12) {
    std::string extensions = "ext1 +cl_khr_gl_msaa_sharing ext2";
    std::string compileOptions = "-cl-std=CL1.1";
    std::string internalOptions = "";
    removeNotSupportedExtensions(extensions, compileOptions, internalOptions);
    EXPECT_EQ(extensions.find("+cl_khr_gl_msaa_sharing"), std::string::npos);

    // Should not remove for 1.2
    extensions = "ext1 +cl_khr_gl_msaa_sharing ext2";
    compileOptions = "-cl-std=CL1.2";
    removeNotSupportedExtensions(extensions, compileOptions, internalOptions);
    EXPECT_NE(extensions.find("+cl_khr_gl_msaa_sharing"), std::string::npos);
}

TEST(CompilerOptionsParser, GivenNoClStdOptionWhenRequiresOpenClCFeaturesAndRequiresAdditionalExtensionsThenFalse) {
    std::string options = "-other-option";
    EXPECT_FALSE(requiresOpenClCFeatures(options));
    EXPECT_FALSE(requiresAdditionalExtensions(options));
}

TEST(CompilerOptionsParser, GivenExtensionsWithoutMsaaSharingWhenRemoveNotSupportedExtensionsThenNoChange) {
    std::string extensions = "ext1 ext2";
    std::string compileOptions = "-cl-std=CL1.1";
    std::string internalOptions = "";
    removeNotSupportedExtensions(extensions, compileOptions, internalOptions);
    EXPECT_EQ(extensions, "ext1 ext2");
}

TEST(CompilerOptionsParser, GivenAppendAdditionalExtensionsWithNoMatchThenNoChange) {
    std::string extensions = "ext1 ";
    std::string compileOptions = "-cl-std=CL1.2";
    std::string internalOptions = "";
    appendAdditionalExtensions(extensions, compileOptions, internalOptions);
    EXPECT_EQ(extensions, "ext1 ");
}

TEST(CompilerOptionsParser, GivenCurrentPolicyAlreadyPresentWhenReplaceL1CachePolicyThenReturnFalse) {
    std::string buildOptions = "-cl-store-cache-default=2 -other-option";
    bool replaced = replaceL1CachePolicyInBuildOptions(buildOptions, "-cl-store-cache-default=2");
    EXPECT_FALSE(replaced);
    EXPECT_EQ(buildOptions, "-cl-store-cache-default=2 -other-option");
}

TEST(CompilerOptionsParser, GivenCurrentPolicyNotPresentWhenReplaceL1CachePolicyAndPrefixFoundThenReplace) {
    std::string buildOptions = "-cl-store-cache-default=1 -other-option";
    bool replaced = replaceL1CachePolicyInBuildOptions(buildOptions, "-cl-store-cache-default=2");
    EXPECT_TRUE(replaced);
    EXPECT_NE(buildOptions.find("-cl-store-cache-default=2"), std::string::npos);
    EXPECT_EQ(buildOptions.find("-cl-store-cache-default=1"), std::string::npos);
}

TEST(CompilerOptionsParser, GivenCurrentPolicyNullptrWhenReplaceL1CachePolicyThenReturnFalse) {
    std::string buildOptions = "-cl-store-cache-default=1 -other-option";
    bool replaced = replaceL1CachePolicyInBuildOptions(buildOptions, nullptr);
    EXPECT_FALSE(replaced);
    EXPECT_EQ(buildOptions, "-cl-store-cache-default=1 -other-option");
}

TEST(CompilerOptionsParser, GivenPrefixNotFoundWhenReplaceL1CachePolicyThenReturnTrue) {
    std::string buildOptions = "-other-option";
    bool replaced = replaceL1CachePolicyInBuildOptions(buildOptions, "-cl-store-cache-default=2");
    EXPECT_TRUE(replaced);
    EXPECT_EQ(buildOptions, "-other-option");
}
