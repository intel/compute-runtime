/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(ReleaseHelperTest, givenReleaseHelper1255ThenCorrectPropertiesAreReturned) {
    HardwareIpVersion ipVersion{};
    ipVersion.architecture = 12;
    ipVersion.release = 55;
    for (auto &revision : {0, 1, 4, 8}) {
        ipVersion.revision = revision;
        auto releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        EXPECT_FALSE(releaseHelper->isAdjustWalkOrderAvailable());
        EXPECT_TRUE(releaseHelper->isMatrixMultiplyAccumulateSupported());
        EXPECT_FALSE(releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired());
        EXPECT_TRUE(releaseHelper->isProgramAllStateComputeCommandFieldsWARequired());
        EXPECT_EQ(revision < 4, releaseHelper->isPrefetchDisablingRequired());
        EXPECT_TRUE(releaseHelper->isSplitMatrixMultiplyAccumulateSupported());
        EXPECT_TRUE(releaseHelper->isBFloat16ConversionSupported());
    }
}

TEST(ReleaseHelperTest, givenReleaseHelper1255ThenMaxPreferredSlmSizeIsNotModified) {
    HardwareIpVersion ipVersion{};
    ipVersion.architecture = 12;
    ipVersion.release = 55;
    for (auto &revision : {0, 1, 4, 8}) {
        ipVersion.revision = revision;
        auto releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        for (auto i = 0; i < 10; i++) {
            auto preferredEnumValue = i;
            auto expectedEnumValue = i;
            EXPECT_EQ(expectedEnumValue, releaseHelper->getProductMaxPreferredSlmSize(preferredEnumValue));
        }
    }
}

TEST(ReleaseHelperTest, givenReleaseHelper1255ThenMediaFrequencyTileIndexIsNotReturned) {
    HardwareIpVersion ipVersion{};
    ipVersion.architecture = 12;
    ipVersion.release = 55;
    for (auto &revision : {0, 1, 4, 8}) {
        ipVersion.revision = revision;
        auto releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        auto tileIndex = 0u;
        auto expectedTileIndex = 0u;
        EXPECT_FALSE(releaseHelper->getMediaFrequencyTileIndex(tileIndex));
        EXPECT_EQ(expectedTileIndex, tileIndex);
    }
}
