/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"
#include "shared/test/unit_test/release_helper/release_helper_tests_base.h"

#include "gtest/gtest.h"

struct ReleaseHelper1274Tests : public ReleaseHelperTests<12, 74> {

    std::vector<uint32_t> getRevisions() override {
        return {0, 4};
    }
};

TEST_F(ReleaseHelper1274Tests, whenGettingCapabilitiesThenCorrectPropertiesAreReturned) {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        EXPECT_TRUE(releaseHelper->isAdjustWalkOrderAvailable());
        EXPECT_TRUE(releaseHelper->isMatrixMultiplyAccumulateSupported());
        EXPECT_TRUE(releaseHelper->isDotProductAccumulateSystolicSupported());
        EXPECT_FALSE(releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired());
        EXPECT_TRUE(releaseHelper->isPipeControlPriorToPipelineSelectWaRequired());
        EXPECT_FALSE(releaseHelper->isProgramAllStateComputeCommandFieldsWARequired());
        EXPECT_FALSE(releaseHelper->isPrefetchDisablingRequired());
        EXPECT_TRUE(releaseHelper->isSplitMatrixMultiplyAccumulateSupported());
        EXPECT_TRUE(releaseHelper->isBFloat16ConversionSupported());
        EXPECT_TRUE(releaseHelper->isResolvingSubDeviceIDNeeded());
        EXPECT_TRUE(releaseHelper->isCachingOnCpuAvailable());
        EXPECT_FALSE(releaseHelper->isDirectSubmissionSupported());
        EXPECT_FALSE(releaseHelper->isAuxSurfaceModeOverrideRequired());
        EXPECT_FALSE(releaseHelper->isRcsExposureDisabled());
        EXPECT_TRUE(releaseHelper->isBindlessAddressingDisabled());
        EXPECT_EQ(8u, releaseHelper->getNumThreadsPerEu());
    }
}

TEST_F(ReleaseHelper1274Tests, whenGettingMaxPreferredSlmSizeThenSizeIsNotModified) {
    whenGettingMaxPreferredSlmSizeThenSizeIsNotModified();
}

TEST_F(ReleaseHelper1274Tests, whenGettingMediaFrequencyTileIndexThenOneIsReturned) {
    whenGettingMediaFrequencyTileIndexThenOneIsReturned();
}

TEST_F(ReleaseHelper1274Tests, whenGettingPreferredAllocationMethodThenNoPreferenceIsReturned) {
    whenGettingPreferredAllocationMethodThenNoPreferenceIsReturned();
}

TEST_F(ReleaseHelper1274Tests, whenShouldAdjustCalledThenFalseReturned) {
    whenShouldAdjustCalledThenFalseReturned();
}

TEST_F(ReleaseHelper1274Tests, whenGettingSupportedNumGrfsThenCorrectValuesAreReturned) {
    whenGettingSupportedNumGrfsThenValues128And256Returned();
}

TEST_F(ReleaseHelper1274Tests, whenGettingThreadsPerEuConfigsThen4And8AreReturned) {
    whenGettingThreadsPerEuConfigsThen4And8AreReturned();
}