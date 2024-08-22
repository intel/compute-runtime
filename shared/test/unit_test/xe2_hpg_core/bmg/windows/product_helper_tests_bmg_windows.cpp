/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe2_hpg_core/hw_cmds_bmg.h"
#include "shared/source/xe2_hpg_core/hw_info_xe2_hpg_core.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

#include "per_product_test_definitions.h"

using namespace NEO;

using BmgProductHelperWindows = ProductHelperTest;

BMGTEST_F(BmgProductHelperWindows, givenProductHelperWhenCheckDirectSubmissionSupportedThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isDirectSubmissionSupported(releaseHelper));
}

BMGTEST_F(BmgProductHelperWindows, givenProductHelperWhenIsStagingBuffersEnabledThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isStagingBuffersEnabled());
}

BMGTEST_F(BmgProductHelperWindows, givenProductHelperWhenOverridePatIndexCalledThenCorrectValueIsReturned) {
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
}