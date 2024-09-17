/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using TigerlakeLpOnlyTest = ::testing::Test;

HWTEST2_F(TigerlakeLpOnlyTest, WhenGettingHardwareInfoThenProductFamilyIsTigerlakeLp, IsTGLLP) {
    EXPECT_EQ(IGFX_TIGERLAKE_LP, defaultHwInfo->platform.eProductFamily);
}

using Gen12LpOnlyTest = ::testing::Test;

GEN12LPTEST_F(Gen12LpOnlyTest, WhenGettingRenderCoreFamilyThenGen12lpCoreIsReturned) {
    EXPECT_NE(IGFX_XE2_HPG_CORE, defaultHwInfo->platform.eRenderCoreFamily);
}
