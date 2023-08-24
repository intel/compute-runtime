/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(ReleaseHelperTest, givenReleaseHelper1271ThenCorrectPropertiesAreReturned) {
    HardwareIpVersion ipVersion{};
    ipVersion.architecture = 12;
    ipVersion.release = 71;
    for (auto &revision : {0, 4}) {
        ipVersion.revision = revision;
        auto releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        EXPECT_FALSE(releaseHelper->isAdjustWalkOrderAvailable());
        EXPECT_FALSE(releaseHelper->isMatrixMultiplyAccumulateSupported());
        EXPECT_EQ(revision == 0, releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired());
        EXPECT_EQ(revision == 0, releaseHelper->isProgramAllStateComputeCommandFieldsWARequired());
        EXPECT_FALSE(releaseHelper->isPrefetchDisablingRequired());
        EXPECT_FALSE(releaseHelper->isSplitMatrixMultiplyAccumulateSupported());
        EXPECT_FALSE(releaseHelper->isBFloat16ConversionSupported());
    }
}

TEST(ReleaseHelperTest, givenReleaseHelper1271ThenMaxPreferredSlmSizeIsLimitedBy96K) {
    HardwareIpVersion ipVersion{};
    ipVersion.architecture = 12;
    ipVersion.release = 71;
    for (auto &revision : {0, 4}) {
        ipVersion.revision = revision;
        auto releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        using PREFERRED_SLM_ALLOCATION_SIZE = typename XeHpgCoreFamily::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;
        for (auto &preferredSlmSize : {PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0K,
                                       PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_16K,
                                       PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_32K,
                                       PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_64K,
                                       PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_96K}) {

            auto maxPreferredSlmValue = releaseHelper->getProductMaxPreferredSlmSize(preferredSlmSize);
            EXPECT_EQ(maxPreferredSlmValue, preferredSlmSize);
        }
        auto preferredSlmSize96k = PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_96K;
        auto preferredSlmSize128k = PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_128K;
        auto maxPreferredSlmValue = releaseHelper->getProductMaxPreferredSlmSize(preferredSlmSize128k);
        EXPECT_EQ(maxPreferredSlmValue, preferredSlmSize96k);
    }
}

TEST(ReleaseHelperTest, givenReleaseHelper1271ThenCorrectMediaFrequencyTileIndexIsReturned) {
    HardwareIpVersion ipVersion{};
    ipVersion.architecture = 12;
    ipVersion.release = 71;
    for (auto &revision : {0, 4}) {
        ipVersion.revision = revision;
        auto releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        auto tileIndex = 0u;
        auto expectedTileIndex = 1u;
        EXPECT_TRUE(releaseHelper->getMediaFrequencyTileIndex(tileIndex));
        EXPECT_EQ(expectedTileIndex, tileIndex);
    }
}
