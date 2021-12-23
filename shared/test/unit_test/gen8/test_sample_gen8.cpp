/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using BroadwellOnlyTest = ::testing::Test;

BDWTEST_F(BroadwellOnlyTest, WhenGettingProductFamilyThenBroadwellIsReturned) {
    EXPECT_EQ(IGFX_BROADWELL, defaultHwInfo->platform.eProductFamily);
}

using Gen8OnlyTest = ::testing::Test;

GEN8TEST_F(Gen8OnlyTest, WhenGettingRenderCoreFamilyThenGen8CoreIsReturned) {
    EXPECT_EQ(IGFX_GEN8_CORE, defaultHwInfo->platform.eRenderCoreFamily);
}
