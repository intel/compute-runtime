/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

using namespace NEO;

using XeHpgProductHelper = ProductHelperTest;

XE_HPG_CORETEST_F(XeHpgProductHelper, givenProductHelperWhenIsSystolicModeConfigurabledThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isSystolicModeConfigurable(pInHwInfo));
}