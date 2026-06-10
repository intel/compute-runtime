/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/unit_test/release_helper/release_helper_tests_base.h"

#include "gtest/gtest.h"

struct ReleaseHelperGen12LpTests : public ReleaseHelperTestsBase, public ::testing::WithParamInterface<uint32_t> {

    void SetUp() override {
        ipVersion.architecture = 12;
        ipVersion.release = GetParam();
    }

    std::vector<uint32_t> getRevisions() override {
        return {0};
    }
};

TEST_P(ReleaseHelperGen12LpTests, whenGettingCapabilitiesThenCorrectPropertiesAreReturned) {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        EXPECT_FALSE(releaseHelper->isAdjustWalkOrderAvailable());
        EXPECT_FALSE(releaseHelper->isMatrixMultiplyAccumulateSupported());
        EXPECT_FALSE(releaseHelper->isDotProductAccumulateSystolicSupported());
        EXPECT_FALSE(releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsBaseWARequired());
        EXPECT_FALSE(releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequired(*defaultHwInfo, false));
        EXPECT_FALSE(releaseHelper->isPipeControlPriorToPipelineSelectWaRequired());
        EXPECT_FALSE(releaseHelper->isProgramAllStateComputeCommandFieldsWARequired());
        EXPECT_FALSE(releaseHelper->isSplitMatrixMultiplyAccumulateSupported());
        EXPECT_FALSE(releaseHelper->isBFloat16ConversionSupported());
        EXPECT_TRUE(releaseHelper->isResolvingSubDeviceIDNeeded());
        EXPECT_FALSE(releaseHelper->isAuxSurfaceModeOverrideRequired());
        EXPECT_FALSE(releaseHelper->isRcsExposureDisabled());
        EXPECT_TRUE(releaseHelper->isBindlessAddressingDisabled());
        EXPECT_FALSE(releaseHelper->isGlobalBindlessAllocatorEnabled());
        EXPECT_EQ(0u, releaseHelper->getStackSizePerRay());
        EXPECT_FALSE(releaseHelper->isRayTracingSupported());
        EXPECT_TRUE(releaseHelper->isNumRtStacksPerDssFixedValue());
        EXPECT_TRUE(releaseHelper->getFtrXe2Compression());
        EXPECT_FALSE(releaseHelper->isAvailableSemaphore64Base());
        EXPECT_FALSE(releaseHelper->isLatePreemptionStartSupportedHelper());
    }
}

TEST_P(ReleaseHelperGen12LpTests, whenGettingSupportedNumGrfsThenValue128Returned) {
    whenGettingSupportedNumGrfsThenValue128Returned();
}

TEST_P(ReleaseHelperGen12LpTests, whenGettingThreadsPerEuConfigsThen4And8AreReturned) {
    whenGettingThreadsPerEuConfigsThen4And8AreReturned();
}

TEST_P(ReleaseHelperGen12LpTests, whenGettingTotalMemBankSizeThenReturn32GB) {
    whenGettingTotalMemBankSizeThenReturn32GB();
}

TEST_P(ReleaseHelperGen12LpTests, whenGettingAdditionalFp16AtomicCapabilitiesThenReturnNoCapabilities) {
    whenGettingAdditionalFp16AtomicCapabilitiesThenReturnNoCapabilities();
}

TEST_P(ReleaseHelperGen12LpTests, whenGettingAdditionalExtraKernelCapabilitiesThenReturnNoCapabilities) {
    whenGettingAdditionalExtraKernelCapabilitiesThenReturnNoCapabilities();
}

TEST_P(ReleaseHelperGen12LpTests, whenIsLocalOnlyAllowedCalledThenTrueReturned) {
    whenIsLocalOnlyAllowedCalledThenTrueReturned();
}

TEST_P(ReleaseHelperGen12LpTests, whenIsDummyBlitWaRequiredCalledThenFalseReturned) {
    whenIsDummyBlitWaRequiredCalledThenFalseReturned();
}

TEST_P(ReleaseHelperGen12LpTests, whenIsBlitImageAllowedForDepthFormatCalledThenTrueReturned) {
    whenIsBlitImageAllowedForDepthFormatCalledThenTrueReturned();
}

TEST_P(ReleaseHelperGen12LpTests, whenProgrammAdditionalStallPriorToBarrierWithTimestampCalledThenFalseReturned) {
    whenProgrammAdditionalStallPriorToBarrierWithTimestampCalledThenFalseReturned();
}

TEST_P(ReleaseHelperGen12LpTests, whenIsPostImageWriteFlushRequiredCalledThenFalseReturned) {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);
        EXPECT_FALSE(releaseHelper->isPostImageWriteFlushRequired());
    }
}

TEST_P(ReleaseHelperGen12LpTests, whenGettingPreferredSlmSizeThenAllEntriesEmpty) {
    whenGettingPreferredSlmSizeThenAllEntriesEmpty();
}

TEST_P(ReleaseHelperGen12LpTests, whenCallingAdjustMaxThreadsPerEuCountThenCorrectValueIsReturned) {
    whenCallingAdjustMaxThreadsPerEuCountThenCorrectValueIsReturned();
}

TEST_P(ReleaseHelperGen12LpTests, whenShouldQueryPeerAccessCalledThenFalseReturned) {
    whenShouldQueryPeerAccessCalledThenFalseReturned();
}

TEST_P(ReleaseHelperGen12LpTests, whenIsSingleDispatchRequiredForMultiCCSCalledThenFalseReturned) {
    whenIsSingleDispatchRequiredForMultiCCSCalledThenFalseReturned();
}

TEST_P(ReleaseHelperGen12LpTests, whenIsStateCacheInvalidationWaRequiredCalledThenFalseReturned) {
    whenIsStateCacheInvalidationWaRequiredCalledThenFalseReturned();
}

INSTANTIATE_TEST_SUITE_P(
    AllGen12LpReleases,
    ReleaseHelperGen12LpTests,
    ::testing::Values(0u, 1u, 2u, 3u, 4u, 10u));
