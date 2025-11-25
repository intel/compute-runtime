/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_options_parser.h"
#include "shared/test/common/test_macros/test.h"
using namespace NEO;

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