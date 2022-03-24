/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"

using HwHelperGen12LpTest = ::testing::Test;
using namespace NEO;

GEN12LPTEST_F(HwHelperGen12LpTest, whenGettingCommandBufferPlacementThenSystemMemoryIsReturned) {
    auto &hwHelper = HwHelper::get(renderCoreFamily);

    EXPECT_TRUE(hwHelper.useSystemMemoryPlacementForCommandBuffer(*defaultHwInfo));
}
