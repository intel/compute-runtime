/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"
#include "shared/test/unit_test/release_helper/release_helper_tests_base.h"

#include "gtest/gtest.h"

struct ReleaseHelper2002Tests : public ReleaseHelperTests<20, 2> {

    std::vector<uint32_t> getRevisions() override {
        return {0};
    }
};

TEST_F(ReleaseHelper2002Tests, whenGettingCapabilitiesThenCorrectPropertiesAreReturned) {
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
        EXPECT_TRUE(releaseHelper->isResolvingSubDeviceIDNeeded());

        EXPECT_FALSE(releaseHelper->isDirectSubmissionSupported());
        EXPECT_TRUE(releaseHelper->isAuxSurfaceModeOverrideRequired());
        EXPECT_TRUE(releaseHelper->isRcsExposureDisabled());
        EXPECT_FALSE(releaseHelper->isBindlessAddressingDisabled());
        EXPECT_EQ(8u, releaseHelper->getNumThreadsPerEu());
        EXPECT_EQ(0u, releaseHelper->getAsyncStackSizePerRay());
        EXPECT_TRUE(releaseHelper->isRayTracingSupported());
    }
}

TEST_F(ReleaseHelper2002Tests, whenGettingSupportedNumGrfsThenCorrectValuesAreReturned) {
    whenGettingSupportedNumGrfsThenValues128And256Returned();
}

TEST_F(ReleaseHelper2002Tests, whenGettingThreadsPerEuConfigsThen4And8AreReturned) {
    whenGettingThreadsPerEuConfigsThen4And8AreReturned();
}

TEST_F(ReleaseHelper2002Tests, whenGettingTotalMemBankSizeThenReturn32GB) {
    whenGettingTotalMemBankSizeThenReturn32GB();
}

TEST_F(ReleaseHelper2002Tests, whenGettingAdditionalFp16AtomicCapabilitiesThenReturnNoCapabilities) {
    whenGettingAdditionalFp16AtomicCapabilitiesThenReturnNoCapabilities();
}

TEST_F(ReleaseHelper2002Tests, whenGettingAdditionalExtraKernelCapabilitiesThenReturnNoCapabilities) {
    whenGettingAdditionalExtraKernelCapabilitiesThenReturnNoCapabilities();
}

TEST_F(ReleaseHelper2002Tests, whenIsLocalOnlyAllowedCalledThenFalseReturned) {
    whenIsLocalOnlyAllowedCalledThenFalseReturned();
}

TEST_F(ReleaseHelper2002Tests, whenIsDummyBlitWaRequiredCalledThenFalseReturned) {
    whenIsDummyBlitWaRequiredCalledThenFalseReturned();
}

TEST_F(ReleaseHelper2002Tests, whenIsBlitImageAllowedForDepthFormatCalledThenTrueReturned) {
    whenIsBlitImageAllowedForDepthFormatCalledThenTrueReturned();
}

TEST_F(ReleaseHelper2002Tests, whenGettingNumThreadsPerEuThenCorrectValueIsReturnedBasedOnOverrideNumThreadsPerEuDebugKey) {
    whenGettingNumThreadsPerEuThenCorrectValueIsReturnedBasedOnOverrideNumThreadsPerEuDebugKey();
}

TEST_F(ReleaseHelper2002Tests, whenIsPostImageWriteFlushRequiredCalledThenFalseReturned) {
    whenIsPostImageWriteFlushRequiredCalledThenFalseReturned();
}

TEST_F(ReleaseHelper2002Tests, whenGettingPreferredSlmSizeThenAllEntriesHaveCorrectValues) {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        constexpr uint32_t kB = 1024;

        auto &preferredSlmValueArray = releaseHelper->getSizeToPreferredSlmValue(false);
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

        EXPECT_EQ(std::numeric_limits<uint32_t>::max(), preferredSlmValueArray[6].upperLimit);
        EXPECT_EQ(6u, preferredSlmValueArray[6].valueToProgram);
    }
}
