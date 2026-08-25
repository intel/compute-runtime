/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/release_helpers/release_helper/release_helper.h"
#include "shared/source/xe2_hpg_core/hw_cmds_base.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/unit_test/release_helpers/release_helper/release_helper_tests_base.h"

#include "gtest/gtest.h"

struct ReleaseHelperLnlTests : public ReleaseHelperTests<20, 4> {

    std::vector<uint32_t> getRevisions() override {
        return {0, 1, 4};
    }
};

TEST_F(ReleaseHelperLnlTests, whenGettingCapabilitiesThenCorrectPropertiesAreReturned) {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        EXPECT_FALSE(releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequired(*defaultHwInfo, false));
        EXPECT_TRUE(releaseHelper->isResolvingSubDeviceIDNeeded());
        EXPECT_EQ(0u, releaseHelper->getStackSizePerRay());
        EXPECT_TRUE(releaseHelper->isLatePreemptionStartSupportedHelper());
    }
}

TEST_F(ReleaseHelperLnlTests, whenGettingSupportedNumGrfsThenCorrectValuesAreReturned) {
    whenGettingSupportedNumGrfsThenValues128And256Returned();
}

TEST_F(ReleaseHelperLnlTests, whenGettingThreadsPerEuConfigsThen4And8AreReturned) {
    whenGettingThreadsPerEuConfigsThen4And8AreReturned();
}

TEST_F(ReleaseHelperLnlTests, whenGettingTotalMemBankSizeThenReturn32GB) {
    whenGettingTotalMemBankSizeThenReturn32GB();
}

TEST_F(ReleaseHelperLnlTests, whenIsBlitImageAllowedForDepthFormatCalledThenTrueReturned) {
    whenIsBlitImageAllowedForDepthFormatCalledThenTrueReturned();
}

TEST_F(ReleaseHelperLnlTests, whenProgrammAdditionalStallPriorToBarrierWithTimestampCalledThenFalseReturned) {
    whenProgrammAdditionalStallPriorToBarrierWithTimestampCalledThenFalseReturned();
}

TEST_F(ReleaseHelperLnlTests, whenIsPostImageWriteFlushRequiredCalledThenFalseReturned) {
    whenIsPostImageWriteFlushRequiredCalledThenFalseReturned();
}

TEST_F(ReleaseHelperLnlTests, whenIsPreImageReadFlushRequiredCalledThenTrueReturned) {
    whenIsPreImageReadFlushRequiredCalledThenTrueReturned();
}

TEST_F(ReleaseHelperLnlTests, whenGettingPreferredSlmSizeThenAllEntriesHaveCorrectValues) {
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

        EXPECT_EQ(std::numeric_limits<uint32_t>::max(), preferredSlmValueArray[5].upperLimit);
        EXPECT_EQ(5u, preferredSlmValueArray[5].valueToProgram);
    }
}

TEST_F(ReleaseHelperLnlTests, whenCallingAdjustMaxThreadsPerEuCountThenCorrectValueIsReturned) {
    whenCallingAdjustMaxThreadsPerEuCountThenCorrectValueIsReturned();
}

TEST_F(ReleaseHelperLnlTests, whenShouldQueryPeerAccessCalledThenFalseReturned) {
    whenShouldQueryPeerAccessCalledThenFalseReturned();
}

TEST_F(ReleaseHelperLnlTests, whenIsSingleDispatchRequiredForMultiCCSCalledThenFalseReturned) {
    whenIsSingleDispatchRequiredForMultiCCSCalledThenFalseReturned();
}

TEST_F(ReleaseHelperLnlTests, whenIsStateCacheInvalidationWaRequiredCalledThenTrueReturned) {
    whenIsStateCacheInvalidationWaRequiredCalledThenTrueReturned();
}

TEST_F(ReleaseHelperLnlTests, whenIsStateCacheInvalidationWaRequiredCalledWithDebugFlagSetThenCorrectValueReturned) {
    whenIsStateCacheInvalidationWaRequiredCalledWithDebugFlagSetThenCorrectValueReturned();
}
