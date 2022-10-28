/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using Gen9OnlyTest = ::testing::Test;

GEN9TEST_F(Gen9OnlyTest, WhenGettingRenderCoreFamilyThenGen9CoreIsReturned) {
    EXPECT_EQ(IGFX_GEN9_CORE, defaultHwInfo->platform.eRenderCoreFamily);
}
