/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/api_specific_config.h"

#include "opencl/source/os_interface/ocl_reg_path.h"

#include "gtest/gtest.h"

namespace NEO {

TEST(ApiSpecificConfigOclTests, WhenGettingApiTypeThenCorrectTypeIsReturned) {
    EXPECT_EQ(ApiSpecificConfig::OCL, ApiSpecificConfig::getApiType());
}

TEST(ApiSpecificConfigOclTests, WhenGettingAUBPrefixByApiTypeOCLIsReturned) {
    EXPECT_EQ(0, strcmp("ocl_", ApiSpecificConfig::getAubPrefixForSpecificApi().c_str()));
}

TEST(ApiSpecificConfigOclTests, WhenGettingNameOCLIsReturned) {
    EXPECT_EQ(0, strcmp("ocl", ApiSpecificConfig::getName().c_str()));
}

TEST(ApiSpecificConfigOclTests, WhenCheckingIfStatelessCompressionIsSupportedThenReturnTrue) {
    EXPECT_TRUE(ApiSpecificConfig::isStatelessCompressionSupported());
}

TEST(ApiSpecificConfigOclTests, givenMaxAllocSizeWhenGettingReducedMaxAllocSizeThenReturnHalfOfThat) {
    EXPECT_EQ(512u, ApiSpecificConfig::getReducedMaxAllocSize(1024));
}

TEST(ApiSpecificConfigOclTests, WhenGettingRegistryPathThenOclRegistryPathIsReturned) {
    EXPECT_STREQ(oclRegPath, ApiSpecificConfig::getRegistryPath());
}
} // namespace NEO
