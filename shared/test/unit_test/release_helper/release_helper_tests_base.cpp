/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/release_helper/release_helper_tests_base.h"

#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/release_helper/release_helper.h"

#include "gtest/gtest.h"

using namespace NEO;

ReleaseHelperTestsBase::ReleaseHelperTestsBase() = default;
ReleaseHelperTestsBase ::~ReleaseHelperTestsBase() = default;

void ReleaseHelperTestsBase::whenGettingMaxPreferredSlmSizeThenSizeIsNotModified() {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);
        for (auto i = 0; i < 10; i++) {
            auto preferredEnumValue = i;
            auto expectedEnumValue = i;
            EXPECT_EQ(expectedEnumValue, releaseHelper->getProductMaxPreferredSlmSize(preferredEnumValue));
        }
    }
}

void ReleaseHelperTestsBase::whenShouldAdjustCalledThenTrueReturned() {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);

        EXPECT_TRUE(releaseHelper->shouldAdjustDepth());
    }
}

void ReleaseHelperTestsBase::whenShouldAdjustCalledThenFalseReturned() {
    for (auto &revision : getRevisions()) {
        ipVersion.revision = revision;
        releaseHelper = ReleaseHelper::create(ipVersion);
        ASSERT_NE(nullptr, releaseHelper);
        EXPECT_FALSE(releaseHelper->shouldAdjustDepth());
    }
}

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
        auto &configs = releaseHelper->getThreadsPerEUConfigs();

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