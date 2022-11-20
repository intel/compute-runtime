/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "hw_cmds_xe_hpg_core_base.h"

namespace L0 {
namespace ult {

HWTEST_EXCLUDE_PRODUCT(L0CoreHelperTest, givenBitmaskWithAttentionBitsForSingleThreadWhenGettingThreadsThenSingleCorrectThreadReturned, IGFX_XE_HPG_CORE);
HWTEST_EXCLUDE_PRODUCT(L0CoreHelperTest, givenBitmaskWithAttentionBitsForAllSubslicesWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_XE_HPG_CORE);
HWTEST_EXCLUDE_PRODUCT(L0CoreHelperTest, givenBitmaskWithAttentionBitsForAllEUsWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_XE_HPG_CORE);
HWTEST_EXCLUDE_PRODUCT(L0CoreHelperTest, givenEu0To1Threads0To3BitmaskWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_XE_HPG_CORE);
HWTEST_EXCLUDE_PRODUCT(L0CoreHelperTest, givenBitmaskWithAttentionBitsForHalfOfThreadsWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_XE_HPG_CORE);

using L0CoreHelperTestXeHpg = ::testing::Test;

XE_HPG_CORETEST_F(L0CoreHelperTestXeHpg, GivenXeHpgWhenCheckingL0HelperForMultiTileCapablePlatformThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0CoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0CoreHelper>();
    EXPECT_FALSE(l0CoreHelper.multiTileCapablePlatform());
}

XE_HPG_CORETEST_F(L0CoreHelperTestXeHpg, GivenXeHpgWhenCheckingL0HelperForCmdListHeapSharingSupportThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0CoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0CoreHelper>();
    EXPECT_TRUE(l0CoreHelper.platformSupportsCmdListHeapSharing());
}

XE_HPG_CORETEST_F(L0CoreHelperTestXeHpg, GivenXeHpgcWhenCheckingL0HelperForStateComputeModeTrackingSupportThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0CoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0CoreHelper>();
    EXPECT_TRUE(l0CoreHelper.platformSupportsStateComputeModeTracking());
}

XE_HPG_CORETEST_F(L0CoreHelperTestXeHpg, GivenXeHpgWhenCheckingL0HelperForFrontEndTrackingSupportThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0CoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0CoreHelper>();
    EXPECT_TRUE(l0CoreHelper.platformSupportsFrontEndTracking());
}

XE_HPG_CORETEST_F(L0CoreHelperTestXeHpg, GivenXeHpgWhenCheckingL0HelperForPipelineSelectTrackingSupportThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0CoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0CoreHelper>();
    EXPECT_TRUE(l0CoreHelper.platformSupportsPipelineSelectTracking());
}

} // namespace ult
} // namespace L0
