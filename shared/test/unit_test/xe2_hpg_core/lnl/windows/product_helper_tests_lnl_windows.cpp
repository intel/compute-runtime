/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe2_hpg_core/hw_info_xe2_hpg_core.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

#include "per_product_test_definitions.h"

using namespace NEO;

using LnlProductHelperWindows = ProductHelperTest;

LNLTEST_F(LnlProductHelperWindows, givenProductHelperWhenCheckIsCopyBufferRectSplitSupportedThenReturnsTrue) {
    EXPECT_TRUE(productHelper->isCopyBufferRectSplitSupported());
}

LNLTEST_F(LnlProductHelperWindows, givenOverrideDirectSubmissionTimeoutsCalledThenTimeoutsAreOverridden) {
    uint64_t timeoutUs{5'000};
    productHelper->overrideDirectSubmissionTimeouts(timeoutUs);
    EXPECT_EQ(timeoutUs, 1'000ull);

    DebugManagerStateRestore restorer{};
    debugManager.flags.DirectSubmissionControllerTimeout.set(2'000);

    productHelper->overrideDirectSubmissionTimeouts(timeoutUs);
    EXPECT_EQ(timeoutUs, 2'000ull);
}

LNLTEST_F(LnlProductHelperWindows, givenProductHelperWhenCallDeferMOCSToPatOnWSLThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->deferMOCSToPatIndex(true));
}
