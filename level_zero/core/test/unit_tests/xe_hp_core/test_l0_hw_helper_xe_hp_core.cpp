/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hp_core/hw_cmds.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForSingleThreadWhenGettingThreadsThenSingleCorrectThreadReturned, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForAllSubslicesWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForAllEUsWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenEu0To1Threads0To3BitmaskWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForHalfOfThreadsWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_XE_HP_CORE);

using L0GfxCoreHelperTestXeHp = ::testing::Test;

XEHPTEST_F(L0GfxCoreHelperTestXeHp, GivenXeHpWhenCheckingL0HelperForMultiTileCapablePlatformThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.multiTileCapablePlatform());
}

XEHPTEST_F(L0GfxCoreHelperTestXeHp, GivenXeHpWhenCheckingL0HelperForCmdListHeapSharingSupportThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsCmdListHeapSharing());
}

XEHPTEST_F(L0GfxCoreHelperTestXeHp, GivenXeHpWhenCheckingL0HelperForStateComputeModeTrackingSupportThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsStateComputeModeTracking());
}

XEHPTEST_F(L0GfxCoreHelperTestXeHp, GivenXeHpWhenCheckingL0HelperForFrontEndTrackingSupportThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsFrontEndTracking());
}

XEHPTEST_F(L0GfxCoreHelperTestXeHp, GivenXeHpWhenCheckingL0HelperForPipelineSelectTrackingSupportThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsPipelineSelectTracking());
}

XEHPTEST_F(L0GfxCoreHelperTestXeHp, GivenXeHpWhenCheckingL0HelperForRayTracingSupportThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsRayTracing());
}

} // namespace ult
} // namespace L0
