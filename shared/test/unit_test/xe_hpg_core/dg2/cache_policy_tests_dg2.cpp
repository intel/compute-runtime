/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_policy.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/helpers/gtest_helpers.h"
#include "shared/test/unit_test/os_interface/hw_info_config_tests.h"

using namespace NEO;
HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTest, givenAtLeastXeHpgCoreWhenGetL1CachePolicyThenReturnCorrectValue_IsAtLeastXeHpgCore, IGFX_XE_HPG_CORE);
DG2TEST_F(HwInfoConfigTest, givenDG2WhenGetL1CachePolicyThenReturnWbPolicy) {
    using GfxFamily = typename HwMapper<IGFX_DG2>::GfxFamily;
    EXPECT_EQ(L1CachePolicyHelper<IGFX_DG2>::getL1CachePolicy(), GfxFamily::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WB);
}

HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTest, givenHwInfoConfigWhenGetL1CachePolicyThenReturnWriteByPass_IsAtLeastXeHpgCore, IGFX_XE_HPG_CORE);
DG2TEST_F(HwInfoConfigTest, givenHwInfoConfigWhenGetL1CachePolicyThenReturnWriteByPass) {
    auto hwInfo = *defaultHwInfo;
    auto hwInfoConfig = HwInfoConfig::get(hwInfo.platform.eProductFamily);

    EXPECT_EQ(hwInfoConfig->getL1CachePolicy(), FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WB);
}

HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTest, givenAtLeastXeHpgCoreWhenGetCachingPolicyOptionsThenReturnWriteByPassPolicyOption_IsAtLeastXeHpgCore, IGFX_XE_HPG_CORE);
DG2TEST_F(HwInfoConfigTest, givenDG2WhenGetCachingPolicyOptionsThenReturnCorrectValue) {
    const char *expectedStr = "-cl-store-cache-default=7 -cl-load-cache-default=4";
    EXPECT_EQ(memcmp(L1CachePolicyHelper<IGFX_DG2>::getCachingPolicyOptions(), expectedStr, strlen(expectedStr)), 0);
}
