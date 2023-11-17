/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"
#include "shared/test/unit_test/release_helper/release_helper_tests_base.h"

#include "gtest/gtest.h"

struct ReleaseHelper1257Tests : public ReleaseHelperTests<12, 57> {

    std::vector<uint32_t> getRevisions() override {
        return {0};
    }
};

TEST_F(ReleaseHelper1257Tests, whenGettingCapabilitiesThenCorrectPropertiesAreReturned) {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        EXPECT_FALSE(releaseHelper->isAdjustWalkOrderAvailable());
        EXPECT_TRUE(releaseHelper->isMatrixMultiplyAccumulateSupported());
        EXPECT_FALSE(releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired());
        EXPECT_FALSE(releaseHelper->isPipeControlPriorToPipelineSelectWaRequired());
        EXPECT_TRUE(releaseHelper->isProgramAllStateComputeCommandFieldsWARequired());
        EXPECT_FALSE(releaseHelper->isPrefetchDisablingRequired());
        EXPECT_TRUE(releaseHelper->isSplitMatrixMultiplyAccumulateSupported());
        EXPECT_TRUE(releaseHelper->isBFloat16ConversionSupported());
        EXPECT_TRUE(releaseHelper->isResolvingSubDeviceIDNeeded());
        EXPECT_TRUE(releaseHelper->isCachingOnCpuAvailable());
        EXPECT_TRUE(releaseHelper->isDirectSubmissionSupported());
        EXPECT_FALSE(releaseHelper->isAuxSurfaceModeOverrideRequired());
        EXPECT_TRUE(releaseHelper->isRcsExposureDisabled());
        EXPECT_TRUE(releaseHelper->isBindlessAddressingDisabled());
        EXPECT_EQ(8u, releaseHelper->getNumThreadsPerEu());
    }
}

TEST_F(ReleaseHelper1257Tests, whenGettingMaxPreferredSlmSizeThenSizeIsNotModified) {
    whenGettingMaxPreferredSlmSizeThenSizeIsNotModified();
}

TEST_F(ReleaseHelper1257Tests, whenGettingMediaFrequencyTileIndexThenFalseIsReturned) {
    whenGettingMediaFrequencyTileIndexThenFalseIsReturned();
}

TEST_F(ReleaseHelper1257Tests, whenGettingPreferredAllocationMethodThenNoPreferenceIsReturned) {
    whenGettingPreferredAllocationMethodThenNoPreferenceIsReturned();
}

TEST_F(ReleaseHelper1257Tests, whenShouldAdjustCalledThenFalseReturned) {
    whenShouldAdjustCalledThenFalseReturned();
}

TEST_F(ReleaseHelper1257Tests, whenGettingSupportedNumGrfsThenCorrectValuesAreReturned) {
    whenGettingSupportedNumGrfsThenValues128And256Returned();
}
