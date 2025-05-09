/*
 * Copyright (C) 2024-2025 Intel Corporation
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

LNLTEST_F(LnlProductHelperWindows, givenProductHelperWhenCheckIsCopyBufferRectSplitSupportedThenReturnsTrue) {
    EXPECT_TRUE(productHelper->isCopyBufferRectSplitSupported());
}

LNLTEST_F(LnlProductHelperWindows, givenOverrideDirectSubmissionTimeoutsCalledThenTimeoutsAreOverridden) {
    auto timeout = std::chrono::microseconds{5'000};
    auto maxTimeout = timeout;
    productHelper->overrideDirectSubmissionTimeouts(timeout, maxTimeout);
    EXPECT_EQ(timeout.count(), 1'000);
    EXPECT_EQ(maxTimeout.count(), 1'000);

    DebugManagerStateRestore restorer{};
    debugManager.flags.DirectSubmissionControllerTimeout.set(2'000);
    debugManager.flags.DirectSubmissionControllerMaxTimeout.set(3'000);

    productHelper->overrideDirectSubmissionTimeouts(timeout, maxTimeout);
    EXPECT_EQ(timeout.count(), 2'000);
    EXPECT_EQ(maxTimeout.count(), 3'000);
}
