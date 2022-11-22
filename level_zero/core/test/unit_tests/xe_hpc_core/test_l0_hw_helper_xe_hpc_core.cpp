/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "hw_cmds_xe_hpc_core_base.h"

namespace L0 {
namespace ult {

using L0HwHelperTestXeHpc = Test<DeviceFixture>;

XE_HPC_CORETEST_F(L0HwHelperTestXeHpc, GivenXeHpcWhenCheckingL0HelperForMultiTileCapablePlatformThenReturnTrue) {
    auto &l0CoreHelper = getHelper<L0CoreHelper>();
    EXPECT_TRUE(l0CoreHelper.multiTileCapablePlatform());
}

XE_HPC_CORETEST_F(L0HwHelperTestXeHpc, GivenHpcPlatformsWhenAlwaysAllocateEventInLocalMemCalledThenReturnTrue) {
    auto &l0CoreHelper = getHelper<L0CoreHelper>();
    EXPECT_TRUE(l0CoreHelper.alwaysAllocateEventInLocalMem());
}

XE_HPC_CORETEST_F(L0HwHelperTestXeHpc, GivenXeHpcWhenCheckingL0HelperForCmdListHeapSharingSupportThenReturnTrue) {
    auto &l0CoreHelper = getHelper<L0CoreHelper>();
    EXPECT_TRUE(l0CoreHelper.platformSupportsCmdListHeapSharing());
}

XE_HPC_CORETEST_F(L0HwHelperTestXeHpc, GivenXeHpcWhenCheckingL0HelperForStateComputeModeTrackingSupportThenReturnTrue) {
    auto &l0CoreHelper = getHelper<L0CoreHelper>();
    EXPECT_TRUE(l0CoreHelper.platformSupportsStateComputeModeTracking());
}

XE_HPC_CORETEST_F(L0HwHelperTestXeHpc, GivenXeHpcWhenCheckingL0HelperForFrontEndTrackingSupportThenReturnTrue) {
    auto &l0CoreHelper = getHelper<L0CoreHelper>();
    EXPECT_TRUE(l0CoreHelper.platformSupportsFrontEndTracking());
}

XE_HPC_CORETEST_F(L0HwHelperTestXeHpc, GivenXeHpcWhenCheckingL0HelperForPipelineSelectTrackingSupportThenReturnTrue) {
    auto &l0CoreHelper = getHelper<L0CoreHelper>();
    EXPECT_TRUE(l0CoreHelper.platformSupportsPipelineSelectTracking());
}

XE_HPC_CORETEST_F(L0HwHelperTestXeHpc, GivenXeHpcWhenCheckingL0HelperForRayTracingSupportThenReturnTrue) {
    auto &l0CoreHelper = getHelper<L0CoreHelper>();
    EXPECT_TRUE(l0CoreHelper.platformSupportsRayTracing());
}

} // namespace ult
} // namespace L0
