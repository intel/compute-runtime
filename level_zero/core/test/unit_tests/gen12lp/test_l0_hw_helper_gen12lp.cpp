/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

HWTEST_EXCLUDE_PRODUCT(L0HwHelperTest, givenBitmaskWithAttentionBitsForSingleThreadWhenGettingThreadsThenSingleCorrectThreadReturned, IGFX_GEN12LP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0HwHelperTest, givenBitmaskWithAttentionBitsForAllSubslicesWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_GEN12LP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0HwHelperTest, givenBitmaskWithAttentionBitsForAllEUsWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_GEN12LP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0HwHelperTest, givenEu0To1Threads0To3BitmaskWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_GEN12LP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0HwHelperTest, givenBitmaskWithAttentionBitsForHalfOfThreadsWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_GEN12LP_CORE);

using L0HwHelperTestGen12Lp = ::testing::Test;

GEN12LPTEST_F(L0HwHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForCmdListHeapSharingSupportThenReturnFalse) {
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
    EXPECT_FALSE(L0::L0HwHelperHw<FamilyType>::get().platformSupportsCmdListHeapSharing(hwInfo));
}

GEN12LPTEST_F(L0HwHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForStateComputeModeTrackingSupportThenReturnFalse) {
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
    EXPECT_FALSE(L0::L0HwHelperHw<FamilyType>::get().platformSupportsStateComputeModeTracking(hwInfo));
}

GEN12LPTEST_F(L0HwHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForFrontEndTrackingSupportThenReturnFalse) {
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
    EXPECT_FALSE(L0::L0HwHelperHw<FamilyType>::get().platformSupportsFrontEndTracking(hwInfo));
}

GEN12LPTEST_F(L0HwHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForPipelineSelectTrackingSupportThenReturnFalse) {
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
    EXPECT_FALSE(L0::L0HwHelperHw<FamilyType>::get().platformSupportsPipelineSelectTracking(hwInfo));
}

} // namespace ult
} // namespace L0
