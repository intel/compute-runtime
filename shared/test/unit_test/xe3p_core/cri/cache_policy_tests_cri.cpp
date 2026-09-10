/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_policy.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe3p_core/hw_cmds_cri.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

#include "per_product_test_definitions.h"

using namespace NEO;

using CriCachePolicy = ProductHelperTest;

CRITEST_F(CriCachePolicy, givenCriWhenGetL1CachePolicyThenReturnWsPolicyAndWbpPolicyForDebuggerState) {
    EXPECT_EQ(L1CachePolicyHelper<IGFX_CRI>::getL1CachePolicy(false), FamilyType::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WS);
    EXPECT_EQ(L1CachePolicyHelper<IGFX_CRI>::getL1CachePolicy(true), FamilyType::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WBP);
}

CRITEST_F(CriCachePolicy, givenCriWhenGetUncachedL1CachePolicyThenReturnUcPolicy) {
    EXPECT_EQ(L1CachePolicyHelper<IGFX_CRI>::getUncachedL1CachePolicy(), FamilyType::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_UC);
}

CRITEST_F(CriCachePolicy, givenCriWhenGetCachingPolicyOptionsThenReturnWriteStreamingPolicyOptions) {
    const char *writeStreamingPolicyOptions = "-cl-store-cache-default=30 -cl-load-cache-default=25";
    const char *writeByPassPolicyOptions = "-cl-store-cache-default=18 -cl-load-cache-default=25";
    EXPECT_EQ(0, memcmp(L1CachePolicyHelper<IGFX_CRI>::getCachingPolicyOptions(false), writeStreamingPolicyOptions, strlen(writeStreamingPolicyOptions)));
    EXPECT_EQ(0, memcmp(L1CachePolicyHelper<IGFX_CRI>::getCachingPolicyOptions(true), writeByPassPolicyOptions, strlen(writeByPassPolicyOptions)));
}

CRITEST_F(CriCachePolicy, givenCriAndDebugFlagOverridingL1CachePolicyWhenGetL1CachePolicyThenReturnOverriddenPolicy) {
    DebugManagerStateRestore restorer;

    debugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(FamilyType::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WB);
    EXPECT_EQ(L1CachePolicyHelper<IGFX_CRI>::getL1CachePolicy(false), FamilyType::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WB);
    EXPECT_EQ(L1CachePolicyHelper<IGFX_CRI>::getL1CachePolicy(true), FamilyType::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WB);

    debugManager.flags.ForceAllResourcesUncached.set(true);
    EXPECT_EQ(L1CachePolicyHelper<IGFX_CRI>::getL1CachePolicy(false), FamilyType::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_UC);
    EXPECT_EQ(L1CachePolicyHelper<IGFX_CRI>::getL1CachePolicy(true), FamilyType::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_UC);
}
