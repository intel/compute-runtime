/*
 * Copyright (C) 2025 Intel Corporation
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

using BmgProductHelperWindows = ProductHelperTest;

BMGTEST_F(BmgProductHelperWindows, givenProductHelperWhenAskedIfIsTlbFlushRequiredThenFalseIsReturned) {
    EXPECT_FALSE(productHelper->isTlbFlushRequired());
}