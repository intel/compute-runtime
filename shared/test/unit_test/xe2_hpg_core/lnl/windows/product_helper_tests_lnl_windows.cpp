/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe2_hpg_core/hw_cmds_lnl.h"
#include "shared/source/xe2_hpg_core/hw_info_xe2_hpg_core.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

#include "per_product_test_definitions.h"

using namespace NEO;

using LnlProductHelperWindows = ProductHelperTest;

LNLTEST_F(LnlProductHelperWindows, givenProductHelperWhenCheckDirectSubmissionSupportedThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isDirectSubmissionSupported(releaseHelper));
}

LNLTEST_F(LnlProductHelperWindows, givenProductHelperWhenOverridePatIndexCalledThenCorrectValueIsReturned) {
    DebugManagerStateRestore restorer;

    uint64_t expectedPatIndex = 2u;
    for (int i = 0; i < static_cast<int>(AllocationType::count); ++i) {
        EXPECT_EQ(expectedPatIndex, productHelper->overridePatIndex(0u, expectedPatIndex, static_cast<AllocationType>(i)));
    }

    expectedPatIndex = 8u;
    for (int i = 0; i < static_cast<int>(AllocationType::count); ++i) {
        if (static_cast<AllocationType>(i) == AllocationType::sharedImage) {
            EXPECT_EQ(13u, productHelper->overridePatIndex(0u, expectedPatIndex, static_cast<AllocationType>(i)));
        } else {
            EXPECT_EQ(expectedPatIndex, productHelper->overridePatIndex(0u, expectedPatIndex, static_cast<AllocationType>(i)));
        }
    }

    debugManager.flags.OverrideUncachedSharedImages.set(0);

    for (int i = 0; i < static_cast<int>(AllocationType::count); ++i) {
        EXPECT_EQ(expectedPatIndex, productHelper->overridePatIndex(0u, expectedPatIndex, static_cast<AllocationType>(i)));
    }

    expectedPatIndex = 6u;
    EXPECT_EQ(expectedPatIndex, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::bufferHostMemory));
    EXPECT_EQ(expectedPatIndex, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::mapAllocation));
    EXPECT_EQ(expectedPatIndex, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::svmCpu));
    EXPECT_EQ(expectedPatIndex, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::svmZeroCopy));
    EXPECT_EQ(expectedPatIndex, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::internalHostMemory));
    EXPECT_EQ(expectedPatIndex, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::timestampPacketTagBuffer));
    EXPECT_EQ(expectedPatIndex, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::tagBuffer));

    debugManager.flags.AllowDcFlush.set(0);

    uint64_t expectedPatIndexOverride = 2u;
    EXPECT_EQ(expectedPatIndexOverride, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::bufferHostMemory));
    EXPECT_EQ(expectedPatIndexOverride, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::mapAllocation));
    EXPECT_EQ(expectedPatIndexOverride, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::svmCpu));
    EXPECT_EQ(expectedPatIndexOverride, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::svmZeroCopy));
    EXPECT_EQ(expectedPatIndexOverride, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::internalHostMemory));
    EXPECT_EQ(expectedPatIndexOverride, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::timestampPacketTagBuffer));
    EXPECT_EQ(expectedPatIndexOverride, productHelper->overridePatIndex(0u, expectedPatIndex, AllocationType::tagBuffer));
}

LNLTEST_F(LnlProductHelperWindows, givenProductHelperWhenIsStagingBuffersEnabledThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isStagingBuffersEnabled());
}