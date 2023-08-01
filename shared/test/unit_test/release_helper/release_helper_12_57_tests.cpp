/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(ReleaseHelperTest, givenReleaseHelper1257ThenCorrectPropertiesAreReturned) {
    HardwareIpVersion ipVersion{};
    ipVersion.architecture = 12;
    ipVersion.release = 57;
    for (auto &revision : {0}) {
        ipVersion.revision = revision;
        auto releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        EXPECT_FALSE(releaseHelper->isAdjustWalkOrderAvailable());
        EXPECT_TRUE(releaseHelper->isMatrixMultiplyAccumulateSupported());
        EXPECT_FALSE(releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired());
        EXPECT_FALSE(releaseHelper->isPrefetchDisablingRequired());
    }
}

TEST(ReleaseHelperTest, givenReleaseHelper1257ThenMaxPreferredSlmSizeIsNotModified) {
    HardwareIpVersion ipVersion{};
    ipVersion.architecture = 12;
    ipVersion.release = 57;
    for (auto &revision : {0}) {
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

TEST(ReleaseHelperTest, givenReleaseHelper1257ThenMediaFrequencyTileIndexIsNotReturned) {
    HardwareIpVersion ipVersion{};
    ipVersion.architecture = 12;
    ipVersion.release = 57;
    for (auto &revision : {0}) {
        ipVersion.revision = revision;
        auto releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        auto tileIndex = 0u;
        auto expectedTileIndex = 0u;
        EXPECT_FALSE(releaseHelper->getMediaFrequencyTileIndex(tileIndex));
        EXPECT_EQ(expectedTileIndex, tileIndex);
    }
}