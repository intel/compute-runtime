/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

HWTEST_EXCLUDE_PRODUCT(L0CoreHelperTest, givenBitmaskWithAttentionBitsForSingleThreadWhenGettingThreadsThenSingleCorrectThreadReturned, IGFX_GEN12LP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0CoreHelperTest, givenBitmaskWithAttentionBitsForAllSubslicesWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_GEN12LP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0CoreHelperTest, givenBitmaskWithAttentionBitsForAllEUsWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_GEN12LP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0CoreHelperTest, givenEu0To1Threads0To3BitmaskWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_GEN12LP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0CoreHelperTest, givenBitmaskWithAttentionBitsForHalfOfThreadsWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_GEN12LP_CORE);

using L0CoreHelperTestGen12Lp = ::testing::Test;

GEN12LPTEST_F(L0CoreHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForCmdListHeapSharingSupportThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0CoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0CoreHelper>();

    EXPECT_FALSE(l0CoreHelper.platformSupportsCmdListHeapSharing());
}

GEN12LPTEST_F(L0CoreHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForStateComputeModeTrackingSupportThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0CoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0CoreHelper>();

    EXPECT_FALSE(l0CoreHelper.platformSupportsStateComputeModeTracking());
}

GEN12LPTEST_F(L0CoreHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForFrontEndTrackingSupportThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0CoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0CoreHelper>();

    EXPECT_FALSE(l0CoreHelper.platformSupportsFrontEndTracking());
}

GEN12LPTEST_F(L0CoreHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForPipelineSelectTrackingSupportThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0CoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0CoreHelper>();

    EXPECT_FALSE(l0CoreHelper.platformSupportsPipelineSelectTracking());
}

GEN12LPTEST_F(L0CoreHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForRayTracingSupportThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment;
    auto &l0CoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<L0CoreHelper>();

    EXPECT_FALSE(l0CoreHelper.platformSupportsRayTracing());
}

} // namespace ult
} // namespace L0
