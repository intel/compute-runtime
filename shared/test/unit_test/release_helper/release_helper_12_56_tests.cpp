/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/release_helper/release_helper.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(ReleaseHelperTest, givenReleaseHelper1256ThenCorrectPropertiesAreReturned) {
    HardwareIpVersion ipVersion{};
    ipVersion.architecture = 12;
    ipVersion.release = 56;
    for (auto &revision : {0, 4, 5}) {
        ipVersion.revision = revision;
        auto releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        EXPECT_FALSE(releaseHelper->isAdjustWalkOrderAvailable());
        EXPECT_TRUE(releaseHelper->isMatrixMultiplyAccumulateSupported());
        EXPECT_FALSE(releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired());
        EXPECT_TRUE(releaseHelper->isProgramAllStateComputeCommandFieldsWARequired());
        EXPECT_FALSE(releaseHelper->isPrefetchDisablingRequired());
        EXPECT_TRUE(releaseHelper->isSplitMatrixMultiplyAccumulateSupported());
        EXPECT_TRUE(releaseHelper->isBFloat16ConversionSupported());
        EXPECT_TRUE(releaseHelper->isResolvingSubDeviceIDNeeded());
    }
}

TEST(ReleaseHelperTest, givenReleaseHelper1256ThenMaxPreferredSlmSizeIsNotModified) {
    HardwareIpVersion ipVersion{};
    ipVersion.architecture = 12;
    ipVersion.release = 56;
    for (auto &revision : {0, 4, 5}) {
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

TEST(ReleaseHelperTest, givenReleaseHelper1256ThenMediaFrequencyTileIndexIsNotReturned) {
    HardwareIpVersion ipVersion{};
    ipVersion.architecture = 12;
    ipVersion.release = 56;
    for (auto &revision : {0, 4, 5}) {
        ipVersion.revision = revision;
        auto releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        auto tileIndex = 0u;
        auto expectedTileIndex = 0u;
        EXPECT_FALSE(releaseHelper->getMediaFrequencyTileIndex(tileIndex));
        EXPECT_EQ(expectedTileIndex, tileIndex);
    }
}

TEST(ReleaseHelperTest, givenReleaseHelper1256WhenCheckPreferredAllocationMethodThenNoPreferenceIsReturned) {
    HardwareIpVersion ipVersion{};
    ipVersion.architecture = 12;
    ipVersion.release = 56;
    for (auto &revision : {0, 4, 5}) {
        ipVersion.revision = revision;
        auto releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);
        for (auto i = 0; i < static_cast<int>(AllocationType::COUNT); i++) {
            auto allocationType = static_cast<AllocationType>(i);
            auto preferredAllocationMethod = releaseHelper->getPreferredAllocationMethod(allocationType);
            EXPECT_FALSE(preferredAllocationMethod.has_value());
        }
    }
}