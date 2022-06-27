/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_policy.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/hw_info_config_tests.h"

using namespace NEO;

HWTEST2_F(HwInfoConfigTest, givenL1CachePolicyHelperWhenUnsupportedL1PoliciesAndGetDefaultL1CachePolicyThenReturnZero, IsAtMostXeHpCore) {
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(), 0u);
}

HWTEST2_F(HwInfoConfigTest, givenAtLeastDG2WhenGetL1CachePolicyThenReturnCorrectValue, IsAtLeastXeHpgCore) {
    using GfxFamily = typename HwMapper<productFamily>::GfxFamily;
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(), GfxFamily::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP);
}

HWTEST2_F(HwInfoConfigTest, givenAtLeastDG2AndWriteBackPolicyWhenGetL1CachePolicyThenReturnCorrectValue, IsAtLeastXeHpgCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(2);

    const char *expectedStr = "-cl-store-cache-default=7 -cl-load-cache-default=4";
    EXPECT_EQ(0, memcmp(L1CachePolicyHelper<productFamily>::getCachingPolicyOptions(), expectedStr, strlen(expectedStr)));
}

HWTEST2_F(HwInfoConfigTest, givenL1CachePolicyHelperWhenDebugFlagSetAndGetL1CachePolicyThenReturnCorrectValue, MatchAny) {
    DebugManagerStateRestore restorer;

    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(0);
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(), 0u);

    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(2);
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(), 2u);

    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(3);
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(), 3u);

    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(4);
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(), 4u);
}
