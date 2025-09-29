/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

using ProductConfigHelperXe2AndLaterTests = ProductHelperTest;

HWTEST2_F(ProductConfigHelperXe2AndLaterTests, givenXe2AndLaterProductHelperWhenCheckDirectSubmissionSupportedThenTrueIsReturned, IsAtLeastXe2HpgCore) {
    EXPECT_TRUE(productHelper->isDirectSubmissionSupported());
}
