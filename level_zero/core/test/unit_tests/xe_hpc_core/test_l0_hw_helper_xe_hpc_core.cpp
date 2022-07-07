/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"

#include "hw_cmds_xe_hpc_core_base.h"

namespace L0 {
namespace ult {

using L0HwHelperTestXeHpc = ::testing::Test;

XE_HPC_CORETEST_F(L0HwHelperTestXeHpc, GivenXeHpcWhenCheckingL0HelperForMultiTileCapablePlatformThenReturnTrue) {
    EXPECT_TRUE(L0::L0HwHelperHw<FamilyType>::get().multiTileCapablePlatform());
}

} // namespace ult
} // namespace L0
