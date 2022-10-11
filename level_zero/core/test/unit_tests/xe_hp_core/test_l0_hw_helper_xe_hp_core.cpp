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

using L0HwHelperTestXeHp = ::testing::Test;

XEHPTEST_F(L0HwHelperTestXeHp, GivenXeHpWhenCheckingL0HelperForMultiTileCapablePlatformThenReturnTrue) {
    EXPECT_TRUE(L0::L0HwHelperHw<FamilyType>::get().multiTileCapablePlatform());
}

XEHPTEST_F(L0HwHelperTestXeHp, GivenXeHpWhenCheckingL0HelperForCmdListHeapSharingSupportThenReturnFalse) {
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
    EXPECT_FALSE(L0::L0HwHelperHw<FamilyType>::get().platformSupportsCmdListHeapSharing(hwInfo));
}

XEHPTEST_F(L0HwHelperTestXeHp, GivenXeHpWhenCheckingL0HelperForStateComputeModeTrackingSupportThenReturnFalse) {
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
    EXPECT_FALSE(L0::L0HwHelperHw<FamilyType>::get().platformSupportsStateComputeModeTracking(hwInfo));
}

XEHPTEST_F(L0HwHelperTestXeHp, GivenXeHpWhenCheckingL0HelperForFrontEndTrackingSupportThenReturnTrue) {
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
    EXPECT_TRUE(L0::L0HwHelperHw<FamilyType>::get().platformSupportsFrontEndTracking(hwInfo));
}

XEHPTEST_F(L0HwHelperTestXeHp, GivenXeHpWhenCheckingL0HelperForPipelineSelectTrackingSupportThenReturnTrue) {
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
    EXPECT_TRUE(L0::L0HwHelperHw<FamilyType>::get().platformSupportsPipelineSelectTracking(hwInfo));
}

} // namespace ult
} // namespace L0
