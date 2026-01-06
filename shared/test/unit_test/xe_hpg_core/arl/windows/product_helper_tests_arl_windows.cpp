/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_info_xe_hpg_core.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/os_interface/windows/product_helper_win_tests.h"

using namespace NEO;

using ArlProductHelperWindows = ProductHelperTestWindows;

ARLTEST_F(ArlProductHelperWindows, givenProductHelperThenCompressionIsNotForbidden) {
    auto hwInfo = *defaultHwInfo;
    EXPECT_FALSE(productHelper->isCompressionForbidden(hwInfo));
}
