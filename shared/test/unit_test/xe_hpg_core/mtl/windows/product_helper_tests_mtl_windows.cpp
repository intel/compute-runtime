/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_info_xe_hpg_core.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/os_interface/windows/product_helper_win_tests.h"

using namespace NEO;

using MtlProductHelperWindows = ProductHelperTestWindows;

MTLTEST_F(MtlProductHelperWindows, whenCheckIsTlbFlushRequiredThenReturnProperValue) {
    EXPECT_TRUE(productHelper->isTlbFlushRequired());
}

MTLTEST_F(MtlProductHelperWindows, whenCheckingIsTimestampWaitSupportedForEventsThenReturnTrue) {
    EXPECT_TRUE(productHelper->isTimestampWaitSupportedForEvents());
}

MTLTEST_F(MtlProductHelperWindows, givenProductHelperWhenIsStagingBuffersEnabledThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isStagingBuffersEnabled());
}

MTLTEST_F(MtlProductHelperWindows, givenProductHelperThenCompressionIsNotForbidden) {
    auto hwInfo = *defaultHwInfo;
    EXPECT_FALSE(productHelper->isCompressionForbidden(hwInfo));
}
