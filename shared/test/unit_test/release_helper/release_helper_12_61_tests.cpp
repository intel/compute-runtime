/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"
#include "shared/test/unit_test/release_helper/release_helper_tests_base.h"

#include "gtest/gtest.h"

struct ReleaseHelper1261Tests : public ReleaseHelperTests<12, 61> {
    std::vector<uint32_t> getRevisions() override {
        return {7};
    }
};

TEST_F(ReleaseHelper1261Tests, whenGettingCapabilitiesThenCorrectPropertiesAreReturned) {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        EXPECT_FALSE(releaseHelper->isAdjustWalkOrderAvailable());
        EXPECT_FALSE(releaseHelper->isMatrixMultiplyAccumulateSupported());
        EXPECT_FALSE(releaseHelper->isDotProductAccumulateSystolicSupported());
        EXPECT_FALSE(releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired());
        EXPECT_FALSE(releaseHelper->isPipeControlPriorToPipelineSelectWaRequired());
        EXPECT_FALSE(releaseHelper->isProgramAllStateComputeCommandFieldsWARequired());
        EXPECT_FALSE(releaseHelper->isPrefetchDisablingRequired());
        EXPECT_FALSE(releaseHelper->isSplitMatrixMultiplyAccumulateSupported());
        EXPECT_FALSE(releaseHelper->isBFloat16ConversionSupported());
        EXPECT_TRUE(releaseHelper->isResolvingSubDeviceIDNeeded());
        EXPECT_TRUE(releaseHelper->isRcsExposureDisabled());
        EXPECT_TRUE(releaseHelper->isBindlessAddressingDisabled());
        EXPECT_EQ(8u, releaseHelper->getNumThreadsPerEu());
        EXPECT_TRUE(releaseHelper->isRayTracingSupported());
        EXPECT_FALSE(releaseHelper->isMidThreadPreemptionDisallowedForRayTracingKernels());
    }
}

TEST_F(ReleaseHelper1261Tests, whenGettingTotalMemBankSizeThenReturn32GB) {
    whenGettingTotalMemBankSizeThenReturn32GB();
}

TEST_F(ReleaseHelper1261Tests, whenGettingAdditionalFp16AtomicCapabilitiesThenReturnNoCapabilities) {
    whenGettingAdditionalFp16AtomicCapabilitiesThenReturnNoCapabilities();
}

TEST_F(ReleaseHelper1261Tests, whenGettingAdditionalExtraKernelCapabilitiesThenReturnNoCapabilities) {
    whenGettingAdditionalExtraKernelCapabilitiesThenReturnNoCapabilities();
}

TEST_F(ReleaseHelper1261Tests, whenIsLocalOnlyAllowedCalledThenTrueReturned) {
    whenIsLocalOnlyAllowedCalledThenTrueReturned();
}

TEST_F(ReleaseHelper1261Tests, whenGettingPreferredSlmSizeThenAllEntriesEmpty) {
    whenGettingPreferredSlmSizeThenAllEntriesEmpty();
}
