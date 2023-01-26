/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/hw_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "hw_cmds_xe_hpg_core_base.h"

namespace L0 {
namespace ult {

HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForSingleThreadWhenGettingThreadsThenSingleCorrectThreadReturned, IGFX_XE_HPG_CORE);
HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForAllSubslicesWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_XE_HPG_CORE);
HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForAllEUsWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_XE_HPG_CORE);
HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenEu0To1Threads0To3BitmaskWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_XE_HPG_CORE);
HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForHalfOfThreadsWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_XE_HPG_CORE);

using L0GfxCoreHelperTestXeHpg = ::testing::Test;

XE_HPG_CORETEST_F(L0GfxCoreHelperTestXeHpg, GivenXeHpgWhenCheckingL0HelperForMultiTileCapablePlatformThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();
    EXPECT_FALSE(l0GfxCoreHelper.multiTileCapablePlatform());
}

XE_HPG_CORETEST_F(L0GfxCoreHelperTestXeHpg, GivenXeHpgWhenCheckingL0HelperForCmdListHeapSharingSupportThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsCmdListHeapSharing());
}

XE_HPG_CORETEST_F(L0GfxCoreHelperTestXeHpg, GivenXeHpgcWhenCheckingL0HelperForStateComputeModeTrackingSupportThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsStateComputeModeTracking());
}

XE_HPG_CORETEST_F(L0GfxCoreHelperTestXeHpg, GivenXeHpgWhenCheckingL0HelperForFrontEndTrackingSupportThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsFrontEndTracking());
}

XE_HPG_CORETEST_F(L0GfxCoreHelperTestXeHpg, GivenXeHpgWhenCheckingL0HelperForPipelineSelectTrackingSupportThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsPipelineSelectTracking());
}

XE_HPG_CORETEST_F(L0GfxCoreHelperTestXeHpg, GivenXeHpgWhenCheckingL0HelperForStateBaseAddressTrackingSupportThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();
    EXPECT_FALSE(l0GfxCoreHelper.platformSupportsStateBaseAddressTracking());
}

XE_HPG_CORETEST_F(L0GfxCoreHelperTestXeHpg, GivenXeHpgWhenCheckingL0HelperForRayTracingSupportThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsRayTracing());
}

} // namespace ult
} // namespace L0
