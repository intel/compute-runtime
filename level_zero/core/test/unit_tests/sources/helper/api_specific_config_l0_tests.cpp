/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/api_specific_config.h"

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

} // namespace NEO
