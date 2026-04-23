/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/release_helper/release_helper_tests_base.h"

#include "gtest/gtest.h"

struct ReleaseHelper3511Tests : public ReleaseHelperTests<35, 11> {

    std::vector<uint32_t> getRevisions() override {
        return {0};
    }
};

TEST_F(ReleaseHelper3511Tests, whenGettingCapabilitiesThenCorrectPropertiesAreReturned) {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        EXPECT_FALSE(releaseHelper->isAdjustWalkOrderAvailable());
        EXPECT_TRUE(releaseHelper->isMatrixMultiplyAccumulateSupported());
        EXPECT_TRUE(releaseHelper->isDotProductAccumulateSystolicSupported());
        EXPECT_FALSE(releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired());
        EXPECT_FALSE(releaseHelper->isPipeControlPriorToPipelineSelectWaRequired());
        EXPECT_FALSE(releaseHelper->isProgramAllStateComputeCommandFieldsWARequired());
        EXPECT_FALSE(releaseHelper->isSplitMatrixMultiplyAccumulateSupported());
        EXPECT_TRUE(releaseHelper->isBFloat16ConversionSupported());
        EXPECT_FALSE(releaseHelper->isResolvingSubDeviceIDNeeded());
        EXPECT_TRUE(releaseHelper->isRcsExposureDisabled());
        EXPECT_TRUE(releaseHelper->isBindlessAddressingDisabled());
        EXPECT_FALSE(releaseHelper->isRayTracingSupported());
        EXPECT_EQ(0u, releaseHelper->getStackSizePerRay());
        EXPECT_TRUE(releaseHelper->isNumRtStacksPerDssFixedValue());
        EXPECT_FALSE(releaseHelper->getFtrXe2Compression());
        EXPECT_FALSE(releaseHelper->isAvailableSemaphore64());
        EXPECT_FALSE(releaseHelper->isLatePreemptionStartSupportedHelper());
    }
}

TEST_F(ReleaseHelper3511Tests, whenGettingSupportedNumGrfsThenCorrectValuesAreReturned) {
    whenGettingSupportedNumGrfsThenValuesUpTo512Returned();
}

TEST_F(ReleaseHelper3511Tests, whenGettingThreadsPerEuConfigsThenCorrectValueIsReturnedBasedOnNumThreadPerEu) {
    whenGettingThreadsPerEuConfigsThenCorrectValueIsReturnedBasedOnNumThreadPerEu();
}

TEST_F(ReleaseHelper3511Tests, whenGettingTotalMemBankSizeThenReturn8GB) {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);
        EXPECT_EQ(8u * MemoryConstants::gigaByte, releaseHelper->getTotalMemBankSize());
    }
}

TEST_F(ReleaseHelper3511Tests, whenGettingAdditionalFp16AtomicCapabilitiesThenReturnAddCapabilities) {
    whenGettingAdditionalFp16AtomicCapabilitiesThenReturnAddCapabilities();
}

TEST_F(ReleaseHelper3511Tests, whenGettingAdditionalExtraKernelCapabilitiesThenReturnAddMinMaxAndLoadStoreCapabilities) {
    whenGettingAdditionalExtraKernelCapabilitiesThenReturnAddMinMaxAndLoadStoreCapabilities();
}

TEST_F(ReleaseHelper3511Tests, whenIsLocalOnlyAllowedCalledThenFalseReturned) {
    whenIsLocalOnlyAllowedCalledThenFalseReturned();
}

TEST_F(ReleaseHelper3511Tests, whenIsDummyBlitWaRequiredCalledThenFalseReturned) {
    whenIsDummyBlitWaRequiredCalledThenFalseReturned();
}

TEST_F(ReleaseHelper3511Tests, whenIsBlitImageAllowedForDepthFormatCalledThenTrueReturned) {
    whenIsBlitImageAllowedForDepthFormatCalledThenTrueReturned();
}

TEST_F(ReleaseHelper3511Tests, whenProgrammAdditionalStallPriorToBarrierWithTimestampCalledThenFalseReturned) {
    whenProgrammAdditionalStallPriorToBarrierWithTimestampCalledThenFalseReturned();
}

TEST_F(ReleaseHelper3511Tests, whenIsPostImageWriteFlushRequiredCalledThenFalseReturned) {
    whenIsPostImageWriteFlushRequiredCalledThenFalseReturned();
}

TEST_F(ReleaseHelper3511Tests, whenGettingPreferredSlmSizeThenAllEntriesHaveCorrectValues) {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        constexpr uint32_t kB = 1024;

        auto &preferredSlmValueArrayHeapless = releaseHelper->getSizeToPreferredSlmValue();
        EXPECT_EQ(0u, preferredSlmValueArrayHeapless[0].upperLimit);
        EXPECT_EQ(0u, preferredSlmValueArrayHeapless[0].valueToProgram);

        EXPECT_EQ(16 * kB, preferredSlmValueArrayHeapless[1].upperLimit);
        EXPECT_EQ(1u, preferredSlmValueArrayHeapless[1].valueToProgram);

        EXPECT_EQ(32 * kB, preferredSlmValueArrayHeapless[2].upperLimit);
        EXPECT_EQ(2u, preferredSlmValueArrayHeapless[2].valueToProgram);

        EXPECT_EQ(64 * kB, preferredSlmValueArrayHeapless[3].upperLimit);
        EXPECT_EQ(3u, preferredSlmValueArrayHeapless[3].valueToProgram);

        EXPECT_EQ(96 * kB, preferredSlmValueArrayHeapless[4].upperLimit);
        EXPECT_EQ(4u, preferredSlmValueArrayHeapless[4].valueToProgram);

        EXPECT_EQ(128 * kB, preferredSlmValueArrayHeapless[5].upperLimit);
        EXPECT_EQ(5u, preferredSlmValueArrayHeapless[5].valueToProgram);

        EXPECT_EQ(160 * kB, preferredSlmValueArrayHeapless[6].upperLimit);
        EXPECT_EQ(6u, preferredSlmValueArrayHeapless[6].valueToProgram);

        EXPECT_EQ(192 * kB, preferredSlmValueArrayHeapless[7].upperLimit);
        EXPECT_EQ(7u, preferredSlmValueArrayHeapless[7].valueToProgram);

        EXPECT_EQ(256 * kB, preferredSlmValueArrayHeapless[8].upperLimit);
        EXPECT_EQ(8u, preferredSlmValueArrayHeapless[8].valueToProgram);

        EXPECT_EQ(320 * kB, preferredSlmValueArrayHeapless[9].upperLimit);
        EXPECT_EQ(9u, preferredSlmValueArrayHeapless[9].valueToProgram);

        EXPECT_EQ(std::numeric_limits<uint32_t>::max(), preferredSlmValueArrayHeapless[10].upperLimit);
        EXPECT_EQ(10u, preferredSlmValueArrayHeapless[10].valueToProgram);
    }
}

TEST_F(ReleaseHelper3511Tests, whenCallingAdjustMaxThreadsPerEuCountThenCorrectValueIsReturned) {
    whenCallingAdjustMaxThreadsPerEuCountThenCorrectValueIsReturned();
}

TEST_F(ReleaseHelper3511Tests, whenShouldQueryPeerAccessCalledThenFalseReturned) {
    whenShouldQueryPeerAccessCalledThenFalseReturned();
}

TEST_F(ReleaseHelper3511Tests, whenIsSingleDispatchRequiredForMultiCCSCalledThenFalseReturned) {
    whenIsSingleDispatchRequiredForMultiCCSCalledThenFalseReturned();
}

TEST_F(ReleaseHelper3511Tests, whenIsStateCacheInvalidationWaRequiredCalledThenFalseReturned) {
    whenIsStateCacheInvalidationWaRequiredCalledThenFalseReturned();
}
