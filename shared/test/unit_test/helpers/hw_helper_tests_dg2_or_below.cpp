/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using GfxCoreHelperDg2OrBelowTests = ::testing::Test;

using isDG2OrBelow = IsAtMostProduct<IGFX_DG2>;
HWTEST2_F(GfxCoreHelperDg2OrBelowTests, WhenGettingIsKmdMigrationSupportedThenFalseIsReturned, isDG2OrBelow) {
    auto &gfxCoreHelper = GfxCoreHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
    EXPECT_FALSE(gfxCoreHelper.isKmdMigrationSupported(*defaultHwInfo));
}