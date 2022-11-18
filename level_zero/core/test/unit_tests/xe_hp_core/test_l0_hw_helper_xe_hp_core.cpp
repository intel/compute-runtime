/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hp_core/hw_cmds.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

HWTEST_EXCLUDE_PRODUCT(L0HwHelperTest, givenBitmaskWithAttentionBitsForSingleThreadWhenGettingThreadsThenSingleCorrectThreadReturned, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0HwHelperTest, givenBitmaskWithAttentionBitsForAllSubslicesWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0HwHelperTest, givenBitmaskWithAttentionBitsForAllEUsWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0HwHelperTest, givenEu0To1Threads0To3BitmaskWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0HwHelperTest, givenBitmaskWithAttentionBitsForHalfOfThreadsWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_XE_HP_CORE);

using L0HwHelperTestXeHp = Test<DeviceFixture>;

XEHPTEST_F(L0HwHelperTestXeHp, GivenXeHpWhenCheckingL0HelperForMultiTileCapablePlatformThenReturnTrue) {
    auto &l0CoreHelper = getHelper<L0CoreHelper>();
    EXPECT_TRUE(l0CoreHelper.multiTileCapablePlatform());
}

XEHPTEST_F(L0HwHelperTestXeHp, GivenXeHpWhenCheckingL0HelperForCmdListHeapSharingSupportThenReturnTrue) {
    auto &l0CoreHelper = getHelper<L0CoreHelper>();
    EXPECT_TRUE(l0CoreHelper.platformSupportsCmdListHeapSharing());
}

XEHPTEST_F(L0HwHelperTestXeHp, GivenXeHpWhenCheckingL0HelperForStateComputeModeTrackingSupportThenReturnTrue) {
    auto &l0CoreHelper = getHelper<L0CoreHelper>();
    EXPECT_TRUE(l0CoreHelper.platformSupportsStateComputeModeTracking());
}

XEHPTEST_F(L0HwHelperTestXeHp, GivenXeHpWhenCheckingL0HelperForFrontEndTrackingSupportThenReturnTrue) {
    auto &l0CoreHelper = getHelper<L0CoreHelper>();
    EXPECT_TRUE(l0CoreHelper.platformSupportsFrontEndTracking());
}

XEHPTEST_F(L0HwHelperTestXeHp, GivenXeHpWhenCheckingL0HelperForPipelineSelectTrackingSupportThenReturnTrue) {
    auto &l0CoreHelper = getHelper<L0CoreHelper>();
    EXPECT_TRUE(l0CoreHelper.platformSupportsPipelineSelectTracking());
}

} // namespace ult
} // namespace L0
