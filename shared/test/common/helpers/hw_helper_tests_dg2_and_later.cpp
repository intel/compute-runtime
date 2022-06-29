/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using HwHelperDg2AndLaterTest = Test<DeviceFixture>;

HWTEST2_F(HwHelperDg2AndLaterTest, GivenUseL1CacheAsTrueWhenCallSetL1CachePolicyThenL1CachePolicyL1CacheControlIsSetProperly, IsAtLeastXeHpgCore) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;

    auto &helper = reinterpret_cast<HwHelperHw<FamilyType> &>(HwHelperHw<FamilyType>::get());

    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;
    bool useL1Cache = true;
    helper.setL1CachePolicy(useL1Cache, &surfaceState, defaultHwInfo.get());
    EXPECT_EQ(RENDER_SURFACE_STATE::L1_CACHE_POLICY_WB, surfaceState.getL1CachePolicyL1CacheControl());
}

HWTEST2_F(HwHelperDg2AndLaterTest, GivenOverrideL1CacheControlInSurfaceStateForScratchSpaceWhenCallSetL1CachePolicyThenL1CachePolicyL1CacheControlIsSetProperly, IsAtLeastXeHpgCore) {
    DebugManagerStateRestore restore;
    DebugManager.flags.OverrideL1CacheControlInSurfaceStateForScratchSpace.set(1);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;

    auto &helper = reinterpret_cast<HwHelperHw<FamilyType> &>(HwHelperHw<FamilyType>::get());

    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;
    bool useL1Cache = true;
    helper.setL1CachePolicy(useL1Cache, &surfaceState, defaultHwInfo.get());
    EXPECT_EQ(RENDER_SURFACE_STATE::L1_CACHE_POLICY_UC, surfaceState.getL1CachePolicyL1CacheControl());
}

HWTEST2_F(HwHelperDg2AndLaterTest, GivenUseL1CacheAsFalseWhenCallSetL1CachePolicyThenL1CachePolicyL1CacheControlIsNotSet, IsAtLeastXeHpgCore) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;

    auto &helper = reinterpret_cast<HwHelperHw<FamilyType> &>(HwHelperHw<FamilyType>::get());
    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;
    bool useL1Cache = false;
    helper.setL1CachePolicy(useL1Cache, &surfaceState, defaultHwInfo.get());
    EXPECT_NE(RENDER_SURFACE_STATE::L1_CACHE_POLICY_WB, surfaceState.getL1CachePolicyL1CacheControl());
}
