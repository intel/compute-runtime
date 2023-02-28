/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForSingleThreadWhenGettingThreadsThenSingleCorrectThreadReturned, IGFX_GEN12LP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForAllSubslicesWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_GEN12LP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForAllEUsWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_GEN12LP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenEu0To1Threads0To3BitmaskWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_GEN12LP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForHalfOfThreadsWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_GEN12LP_CORE);

using L0GfxCoreHelperTestGen12Lp = Test<DeviceFixture>;

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForCmdListHeapSharingSupportThenReturnFalse) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();

    EXPECT_FALSE(l0GfxCoreHelper.platformSupportsCmdListHeapSharing());
}

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForStateComputeModeTrackingSupportThenReturnFalse) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();

    EXPECT_FALSE(l0GfxCoreHelper.platformSupportsStateComputeModeTracking());
}

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForFrontEndTrackingSupportThenReturnFalse) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();

    EXPECT_FALSE(l0GfxCoreHelper.platformSupportsFrontEndTracking());
}

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForPipelineSelectTrackingSupportThenReturnFalse) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();

    EXPECT_FALSE(l0GfxCoreHelper.platformSupportsPipelineSelectTracking());
}

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForStateBaseAddressTrackingSupportThenReturnFalse) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_FALSE(l0GfxCoreHelper.platformSupportsStateBaseAddressTracking());
}

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForRayTracingSupportThenReturnFalse) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();

    EXPECT_FALSE(l0GfxCoreHelper.platformSupportsRayTracing());
}

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenGettingPlatformDefaultHeapAddressModelThenReturnPrivateHeaps) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(NEO::HeapAddressModel::PrivateHeaps, l0GfxCoreHelper.getPlatformHeapAddressModel());
}

} // namespace ult
} // namespace L0
