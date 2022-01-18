/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/helpers/api_specific_config.h"

#include "level_zero/core/source/compiler_interface/l0_reg_path.h"

#include "gtest/gtest.h"

namespace NEO {

TEST(ApiSpecificConfigL0Tests, WhenGettingApiTypeThenCorrectTypeIsReturned) {
    EXPECT_EQ(ApiSpecificConfig::L0, ApiSpecificConfig::getApiType());
}

TEST(ApiSpecificConfigL0Tests, WhenGettingAUBPrefixByApiTypeL0IsReturned) {
    EXPECT_EQ(0, strcmp("l0_", ApiSpecificConfig::getAubPrefixForSpecificApi().c_str()));
}

TEST(ApiSpecificConfigL0Tests, WhenGettingNameL0IsReturned) {
    EXPECT_EQ(0, strcmp("l0", ApiSpecificConfig::getName().c_str()));
}

TEST(ApiSpecificConfigL0Tests, WhenCheckingIfStatelessCompressionIsSupportedThenReturnFalse) {
    EXPECT_FALSE(ApiSpecificConfig::isStatelessCompressionSupported());
}

TEST(ApiSpecificConfigL0Tests, givenMaxAllocSizeWhenGettingReducedMaxAllocSizeThenReturnSameValue) {
    EXPECT_EQ(1024u, ApiSpecificConfig::getReducedMaxAllocSize(1024));
}

TEST(ApiSpecificConfigL0Tests, WhenGettingRegistryPathThenL0RegistryPathIsReturned) {
    EXPECT_STREQ(L0::registryPath, ApiSpecificConfig::getRegistryPath());
}
TEST(ImplicitScalingApiTests, givenLevelZeroApiUsedThenSupportEnabled) {
    EXPECT_TRUE(ImplicitScaling::apiSupport);
}
} // namespace NEO
