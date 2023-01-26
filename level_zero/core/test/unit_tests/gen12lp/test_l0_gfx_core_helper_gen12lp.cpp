/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/hw_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForSingleThreadWhenGettingThreadsThenSingleCorrectThreadReturned, IGFX_GEN12LP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForAllSubslicesWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_GEN12LP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForAllEUsWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_GEN12LP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenEu0To1Threads0To3BitmaskWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_GEN12LP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForHalfOfThreadsWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_GEN12LP_CORE);

using L0GfxCoreHelperTestGen12Lp = ::testing::Test;

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForCmdListHeapSharingSupportThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    EXPECT_FALSE(l0GfxCoreHelper.platformSupportsCmdListHeapSharing());
}

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForStateComputeModeTrackingSupportThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    EXPECT_FALSE(l0GfxCoreHelper.platformSupportsStateComputeModeTracking());
}

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForFrontEndTrackingSupportThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    EXPECT_FALSE(l0GfxCoreHelper.platformSupportsFrontEndTracking());
}

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForPipelineSelectTrackingSupportThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    EXPECT_FALSE(l0GfxCoreHelper.platformSupportsPipelineSelectTracking());
}

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForStateBaseAddressTrackingSupportThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();
    EXPECT_FALSE(l0GfxCoreHelper.platformSupportsStateBaseAddressTracking());
}

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForRayTracingSupportThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0GfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0GfxCoreHelper>();

    EXPECT_FALSE(l0GfxCoreHelper.platformSupportsRayTracing());
}

} // namespace ult
} // namespace L0
