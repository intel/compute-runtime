/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/unit_test/release_helper/release_helper_tests_base.h"

#include "gtest/gtest.h"

struct ReleaseHelperDg2G11Tests : public ReleaseHelperTests<12, 56> {

    std::vector<uint32_t> getRevisions() override {
        return {0, 4, 5};
    }
};

TEST_F(ReleaseHelperDg2G11Tests, whenGettingCapabilitiesThenCorrectPropertiesAreReturned) {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        EXPECT_FALSE(releaseHelper->isAdjustWalkOrderAvailable());
        EXPECT_TRUE(releaseHelper->isMatrixMultiplyAccumulateSupported());
        EXPECT_TRUE(releaseHelper->isDotProductAccumulateSystolicSupported());
        EXPECT_TRUE(releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsBaseWARequired());
        EXPECT_FALSE(releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequired(*defaultHwInfo, false));
        EXPECT_FALSE(releaseHelper->isPipeControlPriorToPipelineSelectWaRequired());
        EXPECT_TRUE(releaseHelper->isProgramAllStateComputeCommandFieldsWARequired());
        EXPECT_TRUE(releaseHelper->isSplitMatrixMultiplyAccumulateSupported());
        EXPECT_TRUE(releaseHelper->isBFloat16ConversionSupported());
        EXPECT_TRUE(releaseHelper->isResolvingSubDeviceIDNeeded());
        EXPECT_FALSE(releaseHelper->isAuxSurfaceModeOverrideRequired());
        EXPECT_TRUE(releaseHelper->isRcsExposureDisabled());
        EXPECT_FALSE(releaseHelper->isBindlessAddressingDisabled());
        EXPECT_TRUE(releaseHelper->isGlobalBindlessAllocatorEnabled());
        EXPECT_EQ(0u, releaseHelper->getStackSizePerRay());
        EXPECT_TRUE(releaseHelper->isRayTracingSupported());
        EXPECT_TRUE(releaseHelper->isNumRtStacksPerDssFixedValue());
        EXPECT_FALSE(releaseHelper->getFtrXe2Compression());
        EXPECT_FALSE(releaseHelper->isAvailableSemaphore64Base());
        EXPECT_FALSE(releaseHelper->isLatePreemptionStartSupportedHelper());
    }
}

TEST_F(ReleaseHelperDg2G11Tests, whenGettingSupportedNumGrfsThenCorrectValuesAreReturned) {
    whenGettingSupportedNumGrfsThenValues128And256Returned();
}

TEST_F(ReleaseHelperDg2G11Tests, whenGettingThreadsPerEuConfigsThen4And8AreReturned) {
    whenGettingThreadsPerEuConfigsThen4And8AreReturned();
}

TEST_F(ReleaseHelperDg2G11Tests, whenGettingTotalMemBankSizeThenReturn32GB) {
    whenGettingTotalMemBankSizeThenReturn32GB();
}

TEST_F(ReleaseHelperDg2G11Tests, whenGettingAdditionalFp16AtomicCapabilitiesThenReturnNoCapabilities) {
    whenGettingAdditionalFp16AtomicCapabilitiesThenReturnNoCapabilities();
}

TEST_F(ReleaseHelperDg2G11Tests, whenGettingAdditionalExtraKernelCapabilitiesThenReturnNoCapabilities) {
    whenGettingAdditionalExtraKernelCapabilitiesThenReturnNoCapabilities();
}

TEST_F(ReleaseHelperDg2G11Tests, whenIsLocalOnlyAllowedCalledThenTrueReturned) {
    whenIsLocalOnlyAllowedCalledThenTrueReturned();
}

TEST_F(ReleaseHelperDg2G11Tests, whenIsDummyBlitWaRequiredCalledThenTrueReturned) {
    whenIsDummyBlitWaRequiredCalledThenTrueReturned();
}

TEST_F(ReleaseHelperDg2G11Tests, whenIsBlitImageAllowedForDepthFormatCalledThenTrueReturned) {
    whenIsBlitImageAllowedForDepthFormatCalledThenTrueReturned();
}

TEST_F(ReleaseHelperDg2G11Tests, whenProgrammAdditionalStallPriorToBarrierWithTimestampCalledThenFalseReturned) {
    whenProgrammAdditionalStallPriorToBarrierWithTimestampCalledThenFalseReturned();
}

TEST_F(ReleaseHelperDg2G11Tests, whenIsPostImageWriteFlushRequiredCalledThenTrueReturned) {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);
        EXPECT_TRUE(releaseHelper->isPostImageWriteFlushRequired());
    }
}

TEST_F(ReleaseHelperDg2G11Tests, whenIsPreImageReadFlushRequiredCalledThenFalseReturned) {
    whenIsPreImageReadFlushRequiredCalledThenFalseReturned();
}

TEST_F(ReleaseHelperDg2G11Tests, whenGettingPreferredSlmSizeThenAllEntriesHaveCorrectValues) {
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

        EXPECT_EQ(std::numeric_limits<uint32_t>::max(), preferredSlmValueArray[4].upperLimit);
        EXPECT_EQ(12u, preferredSlmValueArray[4].valueToProgram);
    }
}

TEST_F(ReleaseHelperDg2G11Tests, whenCallingAdjustMaxThreadsPerEuCountThenCorrectValueIsReturned) {
    whenCallingAdjustMaxThreadsPerEuCountThenCorrectValueIsReturned();
}

TEST_F(ReleaseHelperDg2G11Tests, whenShouldQueryPeerAccessCalledThenFalseReturned) {
    whenShouldQueryPeerAccessCalledThenFalseReturned();
}

TEST_F(ReleaseHelperDg2G11Tests, whenIsSingleDispatchRequiredForMultiCCSCalledThenFalseReturned) {
    whenIsSingleDispatchRequiredForMultiCCSCalledThenFalseReturned();
}

TEST_F(ReleaseHelperDg2G11Tests, whenIsStateCacheInvalidationWaRequiredCalledThenFalseReturned) {
    whenIsStateCacheInvalidationWaRequiredCalledThenFalseReturned();
}

TEST_F(ReleaseHelperDg2G11Tests, whenNumberOfCCSEnabledLargerThanOneAndIsRcsFalseThenPipeControlPriorToNonPipelinedStateCommandsExtendedWARequiredCalledThenTrueReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 2;
    releaseHelper = ReleaseHelper::create(ipVersion);
    EXPECT_TRUE(releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequired(hwInfo, false));
}

TEST_F(ReleaseHelperDg2G11Tests, whenNumberOfCCSEnabledLargerThanOneAndIsRcsTrueThenPipeControlPriorToNonPipelinedStateCommandsExtendedWARequiredCalledThenFalseReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 2;
    releaseHelper = ReleaseHelper::create(ipVersion);
    EXPECT_FALSE(releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequired(hwInfo, true));
}

TEST_F(ReleaseHelperDg2G11Tests, whenNumberOfCCSEnabledEqualToOneAndIsRcsFalseThenPipeControlPriorToNonPipelinedStateCommandsExtendedWARequiredCalledThenFalseReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    releaseHelper = ReleaseHelper::create(ipVersion);
    EXPECT_FALSE(releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequired(hwInfo, false));
}

TEST_F(ReleaseHelperDg2G11Tests, whenNumberOfCCSEnabledEqualToOneAndIsRcsTrueThenPipeControlPriorToNonPipelinedStateCommandsExtendedWARequiredCalledThenFalseReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    releaseHelper = ReleaseHelper::create(ipVersion);
    EXPECT_FALSE(releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequired(hwInfo, true));
}

TEST_F(ReleaseHelperDg2G11Tests, givenDebugFlagSetTo1WhenIsPipeControlPriorToNonPipelinedStateCommandsExtendedWARequiredCalledThenTrueReturnedRegardlessOfCCSAndRcs) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(1);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    releaseHelper = ReleaseHelper::create(ipVersion);
    EXPECT_TRUE(releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequired(hwInfo, true));
}

TEST_F(ReleaseHelperDg2G11Tests, givenDebugFlagSetTo0WhenIsPipeControlPriorToNonPipelinedStateCommandsExtendedWARequiredCalledThenFalseReturnedRegardlessOfCCSAndRcs) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(0);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 2;
    releaseHelper = ReleaseHelper::create(ipVersion);
    EXPECT_FALSE(releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequired(hwInfo, false));
}
