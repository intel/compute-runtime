/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/unit_test/release_helper/release_helper_tests_base.h"

#include "gtest/gtest.h"

struct ReleaseHelperNvlUTests : public ReleaseHelperTests<30, 5> {

    std::vector<uint32_t> getRevisions() override {
        return {0, 1, 4};
    }
};

TEST_F(ReleaseHelperNvlUTests, whenGettingCapabilitiesThenCorrectPropertiesAreReturned) {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        EXPECT_FALSE(releaseHelper->isAdjustWalkOrderAvailable());
        EXPECT_TRUE(releaseHelper->isMatrixMultiplyAccumulateSupported());
        EXPECT_TRUE(releaseHelper->isDotProductAccumulateSystolicSupported());
        EXPECT_FALSE(releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsBaseWARequired());
        EXPECT_FALSE(releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequired(*defaultHwInfo, false));
        EXPECT_FALSE(releaseHelper->isPipeControlPriorToPipelineSelectWaRequired());
        EXPECT_FALSE(releaseHelper->isProgramAllStateComputeCommandFieldsWARequired());
        EXPECT_FALSE(releaseHelper->isSplitMatrixMultiplyAccumulateSupported());
        EXPECT_TRUE(releaseHelper->isBFloat16ConversionSupported());
        EXPECT_TRUE(releaseHelper->isResolvingSubDeviceIDNeeded());
        EXPECT_TRUE(releaseHelper->isRcsExposureDisabled());
        EXPECT_FALSE(releaseHelper->isBindlessAddressingDisabled());
        EXPECT_TRUE(releaseHelper->isGlobalBindlessAllocatorEnabled());
        EXPECT_TRUE(releaseHelper->isRayTracingSupported());
        EXPECT_EQ(64u, releaseHelper->getStackSizePerRay());
        EXPECT_TRUE(releaseHelper->isNumRtStacksPerDssFixedValue());
        EXPECT_TRUE(releaseHelper->getFtrXe2Compression());
        EXPECT_FALSE(releaseHelper->isAvailableSemaphore64Base());
        EXPECT_FALSE(releaseHelper->isLatePreemptionStartSupportedHelper());
    }
}

TEST_F(ReleaseHelperNvlUTests, whenGettingSupportedNumGrfsThenCorrectValuesAreReturned) {
    whenGettingSupportedNumGrfsThenValuesUpTo256Returned();
}

TEST_F(ReleaseHelperNvlUTests, whenGettingThreadsPerEuConfigsThenCorrectValueIsReturnedBasedOnNumThreadPerEu) {
    whenGettingThreadsPerEuConfigsThenCorrectValueIsReturnedBasedOnNumThreadPerEu();
}

TEST_F(ReleaseHelperNvlUTests, whenGettingTotalMemBankSizeThenReturn32GB) {
    whenGettingTotalMemBankSizeThenReturn32GB();
}

TEST_F(ReleaseHelperNvlUTests, whenGettingAdditionalFp16AtomicCapabilitiesThenReturnNoCapabilities) {
    whenGettingAdditionalFp16AtomicCapabilitiesThenReturnNoCapabilities();
}

TEST_F(ReleaseHelperNvlUTests, whenGettingAdditionalExtraKernelCapabilitiesThenReturnNoCapabilities) {
    whenGettingAdditionalExtraKernelCapabilitiesThenReturnNoCapabilities();
}

TEST_F(ReleaseHelperNvlUTests, whenIsLocalOnlyAllowedCalledThenFalseReturned) {
    whenIsLocalOnlyAllowedCalledThenFalseReturned();
}

TEST_F(ReleaseHelperNvlUTests, whenIsDummyBlitWaRequiredCalledThenFalseReturned) {
    whenIsDummyBlitWaRequiredCalledThenFalseReturned();
}

TEST_F(ReleaseHelperNvlUTests, whenIsBlitImageAllowedForDepthFormatCalledThenTrueReturned) {
    whenIsBlitImageAllowedForDepthFormatCalledThenTrueReturned();
}

TEST_F(ReleaseHelperNvlUTests, whenProgrammAdditionalStallPriorToBarrierWithTimestampCalledThenFalseReturned) {
    whenProgrammAdditionalStallPriorToBarrierWithTimestampCalledThenFalseReturned();
}

TEST_F(ReleaseHelperNvlUTests, whenIsPostImageWriteFlushRequiredCalledThenFalseReturned) {
    whenIsPostImageWriteFlushRequiredCalledThenFalseReturned();
}

TEST_F(ReleaseHelperNvlUTests, whenIsPreImageReadFlushRequiredCalledThenTrueReturned) {
    whenIsPreImageReadFlushRequiredCalledThenTrueReturned();
}

TEST_F(ReleaseHelperNvlUTests, whenGettingPreferredSlmSizeThenAllEntriesHaveCorrectValues) {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        constexpr uint32_t kB = 1024;

        auto &preferredSlmValueArray = releaseHelper->getSizeToPreferredSlmValue();
        EXPECT_EQ(0u, preferredSlmValueArray[0].upperLimit);
        EXPECT_EQ(0u, preferredSlmValueArray[0].valueToProgram);

        EXPECT_EQ(16 * kB, preferredSlmValueArray[1].upperLimit);
        EXPECT_EQ(1u, preferredSlmValueArray[1].valueToProgram);

        EXPECT_EQ(32 * kB, preferredSlmValueArray[2].upperLimit);
        EXPECT_EQ(2u, preferredSlmValueArray[2].valueToProgram);

        EXPECT_EQ(64 * kB, preferredSlmValueArray[3].upperLimit);
        EXPECT_EQ(3u, preferredSlmValueArray[3].valueToProgram);

        EXPECT_EQ(96 * kB, preferredSlmValueArray[4].upperLimit);
        EXPECT_EQ(4u, preferredSlmValueArray[4].valueToProgram);

        EXPECT_EQ(128 * kB, preferredSlmValueArray[5].upperLimit);
        EXPECT_EQ(5u, preferredSlmValueArray[5].valueToProgram);

        EXPECT_EQ(160 * kB, preferredSlmValueArray[6].upperLimit);
        EXPECT_EQ(6u, preferredSlmValueArray[6].valueToProgram);

        EXPECT_EQ(std::numeric_limits<uint32_t>::max(), preferredSlmValueArray[7].upperLimit);
        EXPECT_EQ(7u, preferredSlmValueArray[7].valueToProgram);
    }
}

TEST_F(ReleaseHelperNvlUTests, whenCallingAdjustMaxThreadsPerEuCountThenCorrectValueIsReturned) {
    whenCallingAdjustMaxThreadsPerEuCountThenCorrectValueIsReturned();
}

TEST_F(ReleaseHelperNvlUTests, whenShouldQueryPeerAccessCalledThenFalseReturned) {
    whenShouldQueryPeerAccessCalledThenFalseReturned();
}

TEST_F(ReleaseHelperNvlUTests, whenIsSingleDispatchRequiredForMultiCCSCalledThenFalseReturned) {
    whenIsSingleDispatchRequiredForMultiCCSCalledThenFalseReturned();
}

TEST_F(ReleaseHelperNvlUTests, whenIsStateCacheInvalidationWaRequiredCalledThenTrueReturned) {
    whenIsStateCacheInvalidationWaRequiredCalledThenTrueReturned();
}
