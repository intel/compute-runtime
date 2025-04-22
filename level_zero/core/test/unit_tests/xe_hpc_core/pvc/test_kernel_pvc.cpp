/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/hw_test_base.h"

#include "level_zero/core/test/unit_tests/fixtures/kernel_max_cooperative_groups_count_fixture.h"
namespace L0 {
namespace ult {

using KernelImpSuggestMaxCooperativeGroupCountTestsPvc = Test<L0::ult::KernelImpSuggestMaxCooperativeGroupCountFixture>;

PVCTEST_F(KernelImpSuggestMaxCooperativeGroupCountTestsPvc, GivenNoBarriersOrSlmUsedWhenCalculatingMaxCooperativeGroupCountThenResultIsCalculatedWithSimd) {
    auto workGroupSize = lws[0] * lws[1] * lws[2];
    auto expected = availableThreadCount / Math::divideAndRoundUp(workGroupSize, simd);
    EXPECT_EQ(expected, getMaxWorkGroupCount());
}

PVCTEST_F(KernelImpSuggestMaxCooperativeGroupCountTestsPvc, GivenBarriersWhenCalculatingMaxCooperativeGroupCountThenResultIsCalculatedWithRegardToBarriersCount) {
    usesBarriers = 1;
    auto expected = dssCount * (maxBarrierCount / usesBarriers);
    EXPECT_EQ(expected, getMaxWorkGroupCount());
}

PVCTEST_F(KernelImpSuggestMaxCooperativeGroupCountTestsPvc, GivenUsedSlmSizeWhenCalculatingMaxCooperativeGroupCountThenResultIsCalculatedWithRegardToUsedSlmSize) {
    usedSlm = 64 * MemoryConstants::kiloByte;
    auto expected = availableSlm / usedSlm;
    EXPECT_EQ(expected, getMaxWorkGroupCount());
}

} // namespace ult
} // namespace L0
