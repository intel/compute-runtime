/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/release_helper/release_helper_tests_base.h"

#include "gtest/gtest.h"
#include "neo_aot_platforms.h"

struct ReleaseHelper3510Tests : public ReleaseHelperTests<35, 10> {

    std::vector<uint32_t> getRevisions() override {
        return {0, 4};
    }
};

TEST_F(ReleaseHelper3510Tests, whenGettingCapabilitiesThenCorrectPropertiesAreReturned) {
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
        EXPECT_TRUE(releaseHelper->isGlobalBindlessAllocatorEnabled());
        EXPECT_TRUE(releaseHelper->isRayTracingSupported());
        EXPECT_EQ(64u, releaseHelper->getStackSizePerRay());
        EXPECT_FALSE(releaseHelper->isNumRtStacksPerDssFixedValue());
        EXPECT_TRUE(releaseHelper->getFtrXe2Compression());
        EXPECT_FALSE(releaseHelper->isLatePreemptionStartSupportedHelper());
    }
}

TEST_F(ReleaseHelper3510Tests, whenGettingSupportedNumGrfsThenCorrectValuesAreReturned) {
    SupportedNumGrfs expectedValuesRev0{32u, 64u, 96u, 128u, 160u, 192u, 256u, 512u};
    SupportedNumGrfs expectedValues{32u, 64u, 96u, 128u, 160u, 192u, 256u, 320u, 448u, 512u};
    for (auto &baseIpVersion : {static_cast<uint32_t>(AOT::NVL_P_A0)}) {
        ipVersion.value = baseIpVersion;
        for (auto &revision : getRevisions()) {
            ipVersion.revision = revision;
            releaseHelper = ReleaseHelper::create(ipVersion);
            ASSERT_NE(nullptr, releaseHelper);
            if (revision != 0) {
                EXPECT_EQ(expectedValues, releaseHelper->getSupportedNumGrfs());
            } else {
                EXPECT_EQ(expectedValuesRev0, releaseHelper->getSupportedNumGrfs());
            }
        }
    }
}

TEST_F(ReleaseHelper3510Tests, whenGettingThreadsPerEuConfigsThenCorrectValueIsReturnedBasedOnNumThreadPerEu) {
    whenGettingThreadsPerEuConfigsThenCorrectValueIsReturnedBasedOnNumThreadPerEu();
}

TEST_F(ReleaseHelper3510Tests, whenGettingTotalMemBankSizeThenReturn32GB) {
    whenGettingTotalMemBankSizeThenReturn32GB();
}

TEST_F(ReleaseHelper3510Tests, whenGettingAdditionalFp16AtomicCapabilitiesThenReturnAddCapabilities) {
    whenGettingAdditionalFp16AtomicCapabilitiesThenReturnAddCapabilities();
}

TEST_F(ReleaseHelper3510Tests, whenGettingAdditionalExtraKernelCapabilitiesThenReturnAddMinMaxAndLoadStoreCapabilities) {
    whenGettingAdditionalExtraKernelCapabilitiesThenReturnAddMinMaxAndLoadStoreCapabilities();
}

TEST_F(ReleaseHelper3510Tests, whenIsLocalOnlyAllowedCalledThenFalseReturned) {
    whenIsLocalOnlyAllowedCalledThenFalseReturned();
}

TEST_F(ReleaseHelper3510Tests, whenIsDummyBlitWaRequiredCalledThenFalseReturned) {
    whenIsDummyBlitWaRequiredCalledThenFalseReturned();
}

TEST_F(ReleaseHelper3510Tests, whenIsBlitImageAllowedForDepthFormatCalledThenTrueReturned) {
    whenIsBlitImageAllowedForDepthFormatCalledThenTrueReturned();
}

TEST_F(ReleaseHelper3510Tests, whenProgrammAdditionalStallPriorToBarrierWithTimestampCalledThenFalseReturned) {
    whenProgrammAdditionalStallPriorToBarrierWithTimestampCalledThenFalseReturned();
}

TEST_F(ReleaseHelper3510Tests, whenIsPostImageWriteFlushRequiredCalledThenFalseReturned) {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);
        EXPECT_TRUE(releaseHelper->isPostImageWriteFlushRequired());
    }
}

TEST_F(ReleaseHelper3510Tests, whenGettingPreferredSlmSizeThenAllEntriesHaveCorrectValues) {
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

        EXPECT_EQ(std::numeric_limits<uint32_t>::max(), preferredSlmValueArrayHeapless[7].upperLimit);
        EXPECT_EQ(7u, preferredSlmValueArrayHeapless[7].valueToProgram);
    }
}

TEST_F(ReleaseHelper3510Tests, whenCallingAdjustMaxThreadsPerEuCountThenCorrectValueIsReturned) {
    for (auto &baseIpVersion : {static_cast<uint32_t>(AOT::NVL_P_A0)}) {
        ipVersion.value = baseIpVersion;
        for (auto &revision : getRevisions()) {
            ipVersion.revision = revision;
            releaseHelper = ReleaseHelper::create(ipVersion);
            ASSERT_NE(nullptr, releaseHelper);

            if (revision != 0) {
                {
                    uint32_t maxThreadsPerEuCount = 1;
                    uint32_t grfCount = 448;
                    EXPECT_EQ(2u, releaseHelper->adjustMaxThreadsPerEuCount(maxThreadsPerEuCount, grfCount));
                }

                {
                    uint32_t maxThreadsPerEuCount = 1;
                    uint32_t grfCount = 320;
                    EXPECT_EQ(3u, releaseHelper->adjustMaxThreadsPerEuCount(maxThreadsPerEuCount, grfCount));
                }
            } else {
                {
                    uint32_t maxThreadsPerEuCount = 1;
                    uint32_t grfCount = 448;
                    EXPECT_EQ(maxThreadsPerEuCount, releaseHelper->adjustMaxThreadsPerEuCount(maxThreadsPerEuCount, grfCount));
                }

                {
                    uint32_t maxThreadsPerEuCount = 1;
                    uint32_t grfCount = 320;
                    EXPECT_EQ(maxThreadsPerEuCount, releaseHelper->adjustMaxThreadsPerEuCount(maxThreadsPerEuCount, grfCount));
                }
            }

            {
                uint32_t maxThreadsPerEuCount = 5;
                uint32_t grfCount = 255;
                EXPECT_EQ(maxThreadsPerEuCount, releaseHelper->adjustMaxThreadsPerEuCount(maxThreadsPerEuCount, grfCount));
            }

            {
                uint32_t maxThreadsPerEuCount = 1;
                uint32_t grfCount = 449;
                EXPECT_EQ(maxThreadsPerEuCount, releaseHelper->adjustMaxThreadsPerEuCount(maxThreadsPerEuCount, grfCount));
            }
        }
    }
}

TEST_F(ReleaseHelper3510Tests, whenShouldQueryPeerAccessCalledThenFalseReturned) {
    whenShouldQueryPeerAccessCalledThenFalseReturned();
}

TEST_F(ReleaseHelper3510Tests, whenIsSingleDispatchRequiredForMultiCCSCalledThenFalseReturned) {
    whenIsSingleDispatchRequiredForMultiCCSCalledThenFalseReturned();
}

TEST_F(ReleaseHelper3510Tests, whenIsStateCacheInvalidationWaRequiredCalledThenCorrectValueIsReturned) {
    for (auto &baseIpVersion : {static_cast<uint32_t>(AOT::NVL_P_A0)}) {
        ipVersion.value = baseIpVersion;
        for (auto &revision : getRevisions()) {
            ipVersion.revision = revision;
            releaseHelper = ReleaseHelper::create(ipVersion);
            ASSERT_NE(nullptr, releaseHelper);
            if (revision == 0) {
                EXPECT_TRUE(releaseHelper->isStateCacheInvalidationWaRequired());
            } else {
                EXPECT_FALSE(releaseHelper->isStateCacheInvalidationWaRequired());
            }
        }
    }
}

TEST_F(ReleaseHelper3510Tests, whenIsAvailableSemaphore64CalledThenCorrectValueReturned) {
    for (const auto &baseIpVersion : {static_cast<uint32_t>(AOT::NVL_P_A0)}) {
        ipVersion.value = baseIpVersion;
        for (auto &revision : getRevisions()) {
            ipVersion.revision = revision;
            releaseHelper = ReleaseHelper::create(ipVersion);
            ASSERT_NE(nullptr, releaseHelper);
            if (revision != 0) {
                EXPECT_TRUE(releaseHelper->isAvailableSemaphore64());
            } else {
                EXPECT_FALSE(releaseHelper->isAvailableSemaphore64());
            }
        }
    }
}
