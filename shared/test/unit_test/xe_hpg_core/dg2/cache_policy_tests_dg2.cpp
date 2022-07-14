/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_policy.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/helpers/gtest_helpers.h"
#include "shared/test/unit_test/os_interface/hw_info_config_tests.h"

using namespace NEO;

DG2TEST_F(HwInfoConfigTest, givenDG2WhenGetL1CachePolicyThenReturnWbpPolicy) {
    using GfxFamily = typename HwMapper<IGFX_DG2>::GfxFamily;
    EXPECT_EQ(L1CachePolicyHelper<IGFX_DG2>::getL1CachePolicy(), GfxFamily::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP);
}

DG2TEST_F(HwInfoConfigTest, givenDG2WhenGetCachingPolicyOptionsThenReturnCorrectValue) {
    const char *expectedStr = "-cl-store-cache-default=2 -cl-load-cache-default=4";
    EXPECT_EQ(0, memcmp(L1CachePolicyHelper<IGFX_DG2>::getCachingPolicyOptions(), expectedStr, strlen(expectedStr)));
}