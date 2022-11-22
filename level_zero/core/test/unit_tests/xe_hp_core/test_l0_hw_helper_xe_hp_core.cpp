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

HWTEST_EXCLUDE_PRODUCT(L0CoreHelperTest, givenBitmaskWithAttentionBitsForSingleThreadWhenGettingThreadsThenSingleCorrectThreadReturned, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0CoreHelperTest, givenBitmaskWithAttentionBitsForAllSubslicesWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0CoreHelperTest, givenBitmaskWithAttentionBitsForAllEUsWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0CoreHelperTest, givenEu0To1Threads0To3BitmaskWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0CoreHelperTest, givenBitmaskWithAttentionBitsForHalfOfThreadsWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_XE_HP_CORE);

using L0CoreHelperTestXeHp = ::testing::Test;

XEHPTEST_F(L0CoreHelperTestXeHp, GivenXeHpWhenCheckingL0HelperForMultiTileCapablePlatformThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0CoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0CoreHelper>();
    EXPECT_TRUE(l0CoreHelper.multiTileCapablePlatform());
}

XEHPTEST_F(L0CoreHelperTestXeHp, GivenXeHpWhenCheckingL0HelperForCmdListHeapSharingSupportThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0CoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0CoreHelper>();
    EXPECT_TRUE(l0CoreHelper.platformSupportsCmdListHeapSharing());
}

XEHPTEST_F(L0CoreHelperTestXeHp, GivenXeHpWhenCheckingL0HelperForStateComputeModeTrackingSupportThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0CoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0CoreHelper>();
    EXPECT_TRUE(l0CoreHelper.platformSupportsStateComputeModeTracking());
}

XEHPTEST_F(L0CoreHelperTestXeHp, GivenXeHpWhenCheckingL0HelperForFrontEndTrackingSupportThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0CoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0CoreHelper>();
    EXPECT_TRUE(l0CoreHelper.platformSupportsFrontEndTracking());
}

XEHPTEST_F(L0CoreHelperTestXeHp, GivenXeHpWhenCheckingL0HelperForPipelineSelectTrackingSupportThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0CoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0CoreHelper>();
    EXPECT_TRUE(l0CoreHelper.platformSupportsPipelineSelectTracking());
}

XEHPTEST_F(L0CoreHelperTestXeHp, GivenXeHpWhenCheckingL0HelperForRayTracingSupportThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0CoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0CoreHelper>();
    EXPECT_TRUE(l0CoreHelper.platformSupportsRayTracing());
}

} // namespace ult
} // namespace L0
