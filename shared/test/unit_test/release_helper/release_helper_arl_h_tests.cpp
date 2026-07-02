/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/unit_test/release_helper/release_helper_tests_base.h"

#include "gtest/gtest.h"

struct ReleaseHelperArlHTests : public ReleaseHelperTests<12, 74> {

    std::vector<uint32_t> getRevisions() override {
        return {0, 4};
    }
};

TEST_F(ReleaseHelperArlHTests, whenGettingCapabilitiesThenCorrectPropertiesAreReturned) {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        EXPECT_TRUE(releaseHelper->isAdjustWalkOrderAvailable());
        EXPECT_TRUE(releaseHelper->isMatrixMultiplyAccumulateSupported());
        EXPECT_TRUE(releaseHelper->isDotProductAccumulateSystolicSupported());
        EXPECT_FALSE(releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsBaseWARequired());
        EXPECT_FALSE(releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequired(*defaultHwInfo, false));
        EXPECT_TRUE(releaseHelper->isPipeControlPriorToPipelineSelectWaRequired());
        EXPECT_FALSE(releaseHelper->isProgramAllStateComputeCommandFieldsWARequired());
        EXPECT_FALSE(releaseHelper->isSplitMatrixMultiplyAccumulateSupported());
        EXPECT_TRUE(releaseHelper->isBFloat16ConversionSupported());
        EXPECT_TRUE(releaseHelper->isResolvingSubDeviceIDNeeded());
        EXPECT_FALSE(releaseHelper->isAuxSurfaceModeOverrideRequired());
        EXPECT_FALSE(releaseHelper->isRcsExposureDisabled());
        EXPECT_FALSE(releaseHelper->isBindlessAddressingDisabled());
        EXPECT_TRUE(releaseHelper->isGlobalBindlessAllocatorEnabled());
        EXPECT_EQ(0u, releaseHelper->getStackSizePerRay());
        EXPECT_TRUE(releaseHelper->isRayTracingSupported());
        EXPECT_TRUE(releaseHelper->isNumRtStacksPerDssFixedValue());
        EXPECT_FALSE(releaseHelper->getFtrXe2Compression());
        EXPECT_TRUE(releaseHelper->isDirectSubmissionLightSupported());
        EXPECT_FALSE(releaseHelper->isAvailableSemaphore64());
        EXPECT_FALSE(releaseHelper->isLatePreemptionStartSupportedHelper());
    }
}

TEST_F(ReleaseHelperArlHTests, whenGettingSupportedNumGrfsThenCorrectValuesAreReturned) {
    whenGettingSupportedNumGrfsThenValues128And256Returned();
}

TEST_F(ReleaseHelperArlHTests, whenGettingThreadsPerEuConfigsThen4And8AreReturned) {
    whenGettingThreadsPerEuConfigsThen4And8AreReturned();
}

TEST_F(ReleaseHelperArlHTests, whenGettingTotalMemBankSizeThenReturn32GB) {
    whenGettingTotalMemBankSizeThenReturn32GB();
}

TEST_F(ReleaseHelperArlHTests, whenGettingAdditionalFp16AtomicCapabilitiesThenReturnNoCapabilities) {
    whenGettingAdditionalFp16AtomicCapabilitiesThenReturnNoCapabilities();
}

TEST_F(ReleaseHelperArlHTests, whenGettingAdditionalExtraKernelCapabilitiesThenReturnNoCapabilities) {
    whenGettingAdditionalExtraKernelCapabilitiesThenReturnNoCapabilities();
}

TEST_F(ReleaseHelperArlHTests, whenIsLocalOnlyAllowedCalledThenTrueReturned) {
    whenIsLocalOnlyAllowedCalledThenTrueReturned();
}

TEST_F(ReleaseHelperArlHTests, whenIsDummyBlitWaRequiredCalledThenFalseReturned) {
    whenIsDummyBlitWaRequiredCalledThenFalseReturned();
}

TEST_F(ReleaseHelperArlHTests, whenProgrammAdditionalStallPriorToBarrierWithTimestampCalledThenFalseReturned) {
    whenProgrammAdditionalStallPriorToBarrierWithTimestampCalledThenFalseReturned();
}

TEST_F(ReleaseHelperArlHTests, whenIsPostImageWriteFlushRequiredCalledThenFalseReturned) {
    whenIsPostImageWriteFlushRequiredCalledThenFalseReturned();
}

TEST_F(ReleaseHelperArlHTests, whenIsPreImageReadFlushRequiredCalledThenFalseReturned) {
    whenIsPreImageReadFlushRequiredCalledThenFalseReturned();
}

TEST_F(ReleaseHelperArlHTests, whenGettingPreferredSlmSizeThenAllEntriesHaveCorrectValues) {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        constexpr uint32_t kB = 1024;

        auto &preferredSlmValueArray = releaseHelper->getSizeToPreferredSlmValue();
        EXPECT_EQ(0u, preferredSlmValueArray[0].upperLimit);
        EXPECT_EQ(8u, preferredSlmValueArray[0].valueToProgram);

        EXPECT_EQ(16 * kB, preferredSlmValueArray[1].upperLimit);
        EXPECT_EQ(9u, preferredSlmValueArray[1].valueToProgram);

        EXPECT_EQ(32 * kB, preferredSlmValueArray[2].upperLimit);
        EXPECT_EQ(10u, preferredSlmValueArray[2].valueToProgram);

        EXPECT_EQ(64 * kB, preferredSlmValueArray[3].upperLimit);
        EXPECT_EQ(11u, preferredSlmValueArray[3].valueToProgram);

        EXPECT_EQ(96 * kB, preferredSlmValueArray[4].upperLimit);
        EXPECT_EQ(12u, preferredSlmValueArray[4].valueToProgram);

        EXPECT_EQ(std::numeric_limits<uint32_t>::max(), preferredSlmValueArray[5].upperLimit);
        EXPECT_EQ(13u, preferredSlmValueArray[5].valueToProgram);
    }
}
TEST_F(ReleaseHelperArlHTests, whenIsBlitImageAllowedForDepthFormatCalledThenFalseReturned) {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);
        EXPECT_FALSE(releaseHelper->isBlitImageAllowedForDepthFormat());
    }
}

TEST_F(ReleaseHelperArlHTests, whenCallingAdjustMaxThreadsPerEuCountThenCorrectValueIsReturned) {
    whenCallingAdjustMaxThreadsPerEuCountThenCorrectValueIsReturned();
}

TEST_F(ReleaseHelperArlHTests, whenShouldQueryPeerAccessCalledThenFalseReturned) {
    whenShouldQueryPeerAccessCalledThenFalseReturned();
}

TEST_F(ReleaseHelperArlHTests, whenIsSingleDispatchRequiredForMultiCCSCalledThenFalseReturned) {
    whenIsSingleDispatchRequiredForMultiCCSCalledThenFalseReturned();
}

TEST_F(ReleaseHelperArlHTests, whenIsStateCacheInvalidationWaRequiredCalledThenTrueOnlyForImmediateAndImageOrSampler) {
    whenIsStateCacheInvalidationWaRequiredCalledThenTrueOnlyForImmediateAndImageOrSampler();
}
