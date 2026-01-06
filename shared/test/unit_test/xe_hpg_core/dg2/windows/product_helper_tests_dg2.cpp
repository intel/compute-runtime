/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/os_interface/windows/product_helper_win_tests.h"

using namespace NEO;

using Dg2ProductHelperWindows = ProductHelperTestWindows;

DG2TEST_F(Dg2ProductHelperWindows, givenProductHelperWhenCheckingIsBufferPoolAllocatorSupportedThenCorrectValueIsReturned) {
    EXPECT_TRUE(productHelper->isBufferPoolAllocatorSupported());
}

DG2TEST_F(Dg2ProductHelperWindows, givenProductHelperWhenCheckDirectSubmissionSupportedThenTrueIsReturned) {
    EXPECT_FALSE(productHelper->isDirectSubmissionSupported());
}