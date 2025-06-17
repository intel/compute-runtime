/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/release_helper/release_helper_tests_base.h"

#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "gtest/gtest.h"

using namespace NEO;

ReleaseHelperTestsBase::ReleaseHelperTestsBase() = default;
ReleaseHelperTestsBase ::~ReleaseHelperTestsBase() = default;

void ReleaseHelperTestsBase::whenGettingSupportedNumGrfsThenValues128And256Returned() {
    std::vector<uint32_t> expectedValues{128u, 256u};
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);
        EXPECT_EQ(expectedValues, releaseHelper->getSupportedNumGrfs());
    }
}

void ReleaseHelperTestsBase::whenGettingThreadsPerEuConfigsThen4And8AreReturned() {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);
        auto &configs = releaseHelper->getThreadsPerEUConfigs(8u);

        EXPECT_EQ(2U, configs.size());
        EXPECT_EQ(4U, configs[0]);
        EXPECT_EQ(8U, configs[1]);
    }
}

void ReleaseHelperTestsBase::whenGettingTotalMemBankSizeThenReturn32GB() {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);
        EXPECT_EQ(32u * MemoryConstants::gigaByte, releaseHelper->getTotalMemBankSize());
    }
}

void ReleaseHelperTestsBase::whenGettingAdditionalFp16AtomicCapabilitiesThenReturnNoCapabilities() {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        EXPECT_EQ(0u, releaseHelper->getAdditionalFp16Caps());
    }
}

void ReleaseHelperTestsBase::whenGettingAdditionalExtraKernelCapabilitiesThenReturnNoCapabilities() {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        EXPECT_EQ(0u, releaseHelper->getAdditionalExtraCaps());
    }
}

void ReleaseHelperTestsBase::whenIsLocalOnlyAllowedCalledThenTrueReturned() {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        EXPECT_TRUE(releaseHelper->isLocalOnlyAllowed());
    }
}

void ReleaseHelperTestsBase::whenIsLocalOnlyAllowedCalledThenFalseReturned() {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        EXPECT_FALSE(releaseHelper->isLocalOnlyAllowed());
    }
}

void ReleaseHelperTestsBase::whenGettingPreferredSlmSizeThenAllEntriesEmpty() {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        auto &preferredSlmValueArray = releaseHelper->getSizeToPreferredSlmValue(false);
        for (const auto &elem : preferredSlmValueArray) {
            EXPECT_EQ(0u, elem.upperLimit);
            EXPECT_EQ(0u, elem.valueToProgram);
        }
    }
}

void ReleaseHelperTestsBase::whenGettingSupportedNumGrfsThenValuesUpTo256Returned() {
    std::vector<uint32_t> expectedValues{32u, 64u, 96u, 128u, 160u, 192u, 256u};
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);
        EXPECT_EQ(expectedValues, releaseHelper->getSupportedNumGrfs());
    }
}

void ReleaseHelperTestsBase::whenGettingNumThreadsPerEuThenCorrectValueIsReturnedBasedOnOverrideNumThreadsPerEuDebugKey() {
    DebugManagerStateRestore restorer;
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);
        debugManager.flags.OverrideNumThreadsPerEu.set(7);
        EXPECT_EQ(7u, releaseHelper->getNumThreadsPerEu());
        debugManager.flags.OverrideNumThreadsPerEu.set(8);
        EXPECT_EQ(8u, releaseHelper->getNumThreadsPerEu());
        debugManager.flags.OverrideNumThreadsPerEu.set(10);
        EXPECT_EQ(10u, releaseHelper->getNumThreadsPerEu());
    }
}

void ReleaseHelperTestsBase::whenGettingThreadsPerEuConfigsThenCorrectValueIsReturnedBasedOnNumThreadPerEu() {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);
        {
            auto &configs = releaseHelper->getThreadsPerEUConfigs(8);

            EXPECT_EQ(2U, configs.size());
            EXPECT_EQ(4U, configs[0]);
            EXPECT_EQ(8U, configs[1]);
        }
        {
            auto &configs = releaseHelper->getThreadsPerEUConfigs(10);

            EXPECT_EQ(5U, configs.size());
            EXPECT_EQ(4U, configs[0]);
            EXPECT_EQ(5U, configs[1]);
            EXPECT_EQ(6U, configs[2]);
            EXPECT_EQ(8U, configs[3]);
            EXPECT_EQ(10U, configs[4]);
        }
    }
}

void ReleaseHelperTestsBase::whenIsDummyBlitWaRequiredCalledThenTrueReturned() {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);
        EXPECT_TRUE(releaseHelper->isDummyBlitWaRequired());
    }
}

void ReleaseHelperTestsBase::whenIsDummyBlitWaRequiredCalledThenFalseReturned() {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);
        EXPECT_FALSE(releaseHelper->isDummyBlitWaRequired());
    }
}

void ReleaseHelperTestsBase::whenIsBlitImageAllowedForDepthFormatCalledThenTrueReturned() {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);
        EXPECT_TRUE(releaseHelper->isBlitImageAllowedForDepthFormat());
    }
}

void ReleaseHelperTestsBase::whenProgrammAdditionalStallPriorToBarrierWithTimestampCalledThenFalseReturned() {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);
        EXPECT_FALSE(releaseHelper->programmAdditionalStallPriorToBarrierWithTimestamp());
    }
}

void ReleaseHelperTestsBase::whenIsPostImageWriteFlushRequiredCalledThenFalseReturned() {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);
        EXPECT_FALSE(releaseHelper->isPostImageWriteFlushRequired());
    }
}