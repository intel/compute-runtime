/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/hw_info_config_tests.h"

#include "hw_cmds_xe_hpg_core_base.h"

using namespace NEO;

using XeHpgProductHelper = ProductHelperTest;

XE_HPG_CORETEST_F(XeHpgProductHelper, givenProductHelperWhenIsSystolicModeConfigurabledThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isSystolicModeConfigurable(pInHwInfo));
}
