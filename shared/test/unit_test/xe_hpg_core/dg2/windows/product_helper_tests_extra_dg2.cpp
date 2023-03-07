/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/os_interface/windows/product_helper_win_tests.h"

using namespace NEO;

using Dg2ProductHelperWindows = ProductHelperTestWindows;

HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenCheckingIsUnlockingLockedPtrNecessaryThenReturnFalse, IGFX_DG2);

HWTEST2_F(Dg2ProductHelperWindows, givenProductHelperWhenCheckingIsUnlockingLockedPtrNecessaryThenReturnTrue, IsDG2) {
    EXPECT_TRUE(productHelper->isUnlockingLockedPtrNecessary(outHwInfo));
}
