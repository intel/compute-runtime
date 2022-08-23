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
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(false), 0u);
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(true), 0u);
}

HWTEST2_F(HwInfoConfigTest, givenL1CachePolicyHelperWhenUnsupportedL1PoliciesAndGetUncached1CachePolicyThenReturnOne, IsAtMostXeHpCore) {
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getUncachedL1CachePolicy(), 1u);
}

HWTEST2_F(HwInfoConfigTest, givenAtLeastXeHpgCoreWhenGetL1CachePolicyThenReturnCorrectValue, IsAtLeastXeHpgCore) {
    using GfxFamily = typename HwMapper<productFamily>::GfxFamily;
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(false), GfxFamily::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP);
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(true), GfxFamily::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP);
}

HWTEST2_F(HwInfoConfigTest, givenAtLeastXeHpgCoreWhenGetUncached1CachePolicyThenReturnCorrectValue, IsAtLeastXeHpgCore) {
    using GfxFamily = typename HwMapper<productFamily>::GfxFamily;
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getUncachedL1CachePolicy(), GfxFamily::STATE_BASE_ADDRESS::L1_CACHE_POLICY_UC);
}

HWTEST2_F(HwInfoConfigTest, givenAtLeastXeHpgCoreAndWriteBackPolicyWhenGetL1CachePolicyThenReturnCorrectValue, IsAtLeastXeHpgCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(2);

    const char *expectedStr = "-cl-store-cache-default=7 -cl-load-cache-default=4";
    EXPECT_EQ(0, memcmp(L1CachePolicyHelper<productFamily>::getCachingPolicyOptions(false), expectedStr, strlen(expectedStr)));
    EXPECT_EQ(0, memcmp(L1CachePolicyHelper<productFamily>::getCachingPolicyOptions(true), expectedStr, strlen(expectedStr)));
}

HWTEST2_F(HwInfoConfigTest, givenAtLeastXeHpgCoreAndForceAllResourcesUncachedWhenGetL1CachePolicyThenReturnCorrectValue, IsAtLeastXeHpgCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceAllResourcesUncached.set(true);
    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(4);

    const char *expectedStr = "-cl-store-cache-default=1 -cl-load-cache-default=1";
    EXPECT_EQ(0, memcmp(L1CachePolicyHelper<productFamily>::getCachingPolicyOptions(false), expectedStr, strlen(expectedStr)));
    EXPECT_EQ(0, memcmp(L1CachePolicyHelper<productFamily>::getCachingPolicyOptions(true), expectedStr, strlen(expectedStr)));
}

HWTEST2_F(HwInfoConfigTest, givenL1CachePolicyHelperWhenDebugFlagSetAndGetL1CachePolicyThenReturnCorrectValue, MatchAny) {
    DebugManagerStateRestore restorer;

    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(0);
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(false), 0u);
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(true), 0u);

    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(2);
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(false), 2u);
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(true), 2u);

    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(3);
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(false), 3u);
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(true), 3u);

    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(4);
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(false), 4u);
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(true), 4u);
}
