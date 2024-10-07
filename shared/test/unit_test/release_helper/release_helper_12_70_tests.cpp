/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"
#include "shared/test/unit_test/release_helper/release_helper_tests_base.h"

#include "gtest/gtest.h"

struct ReleaseHelper1270Tests : public ReleaseHelperTests<12, 70> {

    std::vector<uint32_t> getRevisions() override {
        return {0, 4};
    }
};

TEST_F(ReleaseHelper1270Tests, whenGettingCapabilitiesThenCorrectPropertiesAreReturned) {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        EXPECT_FALSE(releaseHelper->isAdjustWalkOrderAvailable());
        EXPECT_FALSE(releaseHelper->isMatrixMultiplyAccumulateSupported());
        EXPECT_FALSE(releaseHelper->isDotProductAccumulateSystolicSupported());
        EXPECT_EQ(revision == 0, releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired());
        EXPECT_FALSE(releaseHelper->isPipeControlPriorToPipelineSelectWaRequired());
        EXPECT_EQ(revision == 0, releaseHelper->isProgramAllStateComputeCommandFieldsWARequired());
        EXPECT_FALSE(releaseHelper->isPrefetchDisablingRequired());
        EXPECT_FALSE(releaseHelper->isSplitMatrixMultiplyAccumulateSupported());
        EXPECT_FALSE(releaseHelper->isBFloat16ConversionSupported());
        EXPECT_TRUE(releaseHelper->isResolvingSubDeviceIDNeeded());
        EXPECT_TRUE(releaseHelper->isDirectSubmissionSupported());
        EXPECT_TRUE(releaseHelper->isAuxSurfaceModeOverrideRequired());
        EXPECT_FALSE(releaseHelper->isRcsExposureDisabled());
        EXPECT_FALSE(releaseHelper->isBindlessAddressingDisabled());
        EXPECT_TRUE(releaseHelper->isGlobalBindlessAllocatorEnabled());
        EXPECT_EQ(8u, releaseHelper->getNumThreadsPerEu());
        EXPECT_TRUE(releaseHelper->isRayTracingSupported());
        EXPECT_FALSE(releaseHelper->isDisablingMsaaRequired());
    }
}

TEST_F(ReleaseHelper1270Tests, whenGettingMaxPreferredSlmSizeThenSizeSizeIsLimitedBy96K) {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);
        using PREFERRED_SLM_ALLOCATION_SIZE = typename XeHpgCoreFamily::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;
        for (auto &preferredSlmSize : {PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0K,
                                       PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_16K,
                                       PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_32K,
                                       PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_64K,
                                       PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_96K}) {

            auto maxPreferredSlmValue = releaseHelper->getProductMaxPreferredSlmSize(preferredSlmSize);
            EXPECT_EQ(maxPreferredSlmValue, preferredSlmSize);
        }
        auto preferredSlmSize96k = PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_96K;
        auto preferredSlmSize128k = PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_128K;
        auto maxPreferredSlmValue = releaseHelper->getProductMaxPreferredSlmSize(preferredSlmSize128k);
        EXPECT_EQ(maxPreferredSlmValue, preferredSlmSize96k);
    }
}

TEST_F(ReleaseHelper1270Tests, whenGettingSupportedNumGrfsThenCorrectValuesAreReturned) {
    whenGettingSupportedNumGrfsThenValues128And256Returned();
}

TEST_F(ReleaseHelper1270Tests, whenGettingThreadsPerEuConfigsThen4And8AreReturned) {
    whenGettingThreadsPerEuConfigsThen4And8AreReturned();
}

TEST_F(ReleaseHelper1270Tests, whenGettingTotalMemBankSizeThenReturn32GB) {
    whenGettingTotalMemBankSizeThenReturn32GB();
}

TEST_F(ReleaseHelper1270Tests, whenGettingAdditionalFp16AtomicCapabilitiesThenReturnNoCapabilities) {
    whenGettingAdditionalFp16AtomicCapabilitiesThenReturnNoCapabilities();
}

TEST_F(ReleaseHelper1270Tests, whenGettingAdditionalExtraKernelCapabilitiesThenReturnNoCapabilities) {
    whenGettingAdditionalExtraKernelCapabilitiesThenReturnNoCapabilities();
}

TEST_F(ReleaseHelper1270Tests, whenIsLocalOnlyAllowedCalledThenTrueReturned) {
    whenIsLocalOnlyAllowedCalledThenTrueReturned();
}

TEST_F(ReleaseHelper1270Tests, whenGettingPreferredSlmSizeThenAllEntriesEmpty) {
    whenGettingPreferredSlmSizeThenAllEntriesEmpty();
}
