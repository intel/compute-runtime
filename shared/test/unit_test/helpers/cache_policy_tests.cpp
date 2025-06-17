/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_policy.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

using namespace NEO;

HWTEST2_F(ProductHelperTest, givenL1CachePolicyHelperWhenUnsupportedL1PoliciesAndGetDefaultL1CachePolicyThenReturnZero, IsGen12LP) {
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(false), 0u);
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(true), 0u);
}

HWTEST2_F(ProductHelperTest, givenL1CachePolicyHelperWhenUnsupportedL1PoliciesAndGetUncached1CachePolicyThenReturnOne, IsGen12LP) {
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getUncachedL1CachePolicy(), 1u);
}

HWTEST2_F(ProductHelperTest, givenAtLeastXeHpgCoreWhenGetL1CachePolicyThenReturnCorrectValue, IsAtLeastXeCore) {
    using GfxFamily = typename HwMapper<productFamily>::GfxFamily;
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(false), GfxFamily::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WBP);
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(true), GfxFamily::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WBP);
}

HWTEST2_F(ProductHelperTest, givenAtLeastXeHpgCoreWhenGetUncached1CachePolicyThenReturnCorrectValue, IsAtLeastXeCore) {
    using GfxFamily = typename HwMapper<productFamily>::GfxFamily;
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getUncachedL1CachePolicy(), GfxFamily::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_UC);
}

HWTEST2_F(ProductHelperTest, givenAtLeastXeHpgCoreAndWriteBackPolicyWhenGetL1CachePolicyThenReturnCorrectValue, IsAtLeastXeCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(2);

    const char *expectedStr = "-cl-store-cache-default=7 -cl-load-cache-default=4";
    EXPECT_EQ(0, memcmp(L1CachePolicyHelper<productFamily>::getCachingPolicyOptions(false), expectedStr, strlen(expectedStr)));
    EXPECT_EQ(0, memcmp(L1CachePolicyHelper<productFamily>::getCachingPolicyOptions(true), expectedStr, strlen(expectedStr)));
}

HWTEST2_F(ProductHelperTest, givenAtLeastXeHpgCoreAndForceAllResourcesUncachedWhenGetL1CachePolicyThenReturnCorrectValue, IsAtLeastXeCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceAllResourcesUncached.set(true);
    debugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(4);

    const char *expectedStr = "-cl-store-cache-default=2 -cl-load-cache-default=2";
    EXPECT_EQ(0, memcmp(L1CachePolicyHelper<productFamily>::getCachingPolicyOptions(false), expectedStr, strlen(expectedStr)));
    EXPECT_EQ(0, memcmp(L1CachePolicyHelper<productFamily>::getCachingPolicyOptions(true), expectedStr, strlen(expectedStr)));
}

HWTEST2_F(ProductHelperTest, givenL1CachePolicyHelperWhenDebugFlagSetAndGetL1CachePolicyThenReturnCorrectValue, MatchAny) {
    DebugManagerStateRestore restorer;

    debugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(0);
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(false), 0u);
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(true), 0u);

    debugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(2);
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(false), 2u);
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(true), 2u);

    debugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(3);
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(false), 3u);
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(true), 3u);

    debugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(4);
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(false), 4u);
    EXPECT_EQ(L1CachePolicyHelper<productFamily>::getL1CachePolicy(true), 4u);
}

HWTEST_F(ProductHelperTest, givenL1CachePolicyInitializedWhenGettingPolicySettingsThenExpectValueMatchProductHelper) {
    L1CachePolicy policy(*productHelper);
    EXPECT_EQ(productHelper->getL1CachePolicy(true), policy.getL1CacheValue(true));
    EXPECT_EQ(productHelper->getL1CachePolicy(false), policy.getL1CacheValue(false));
}
