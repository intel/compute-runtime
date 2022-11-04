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

using HwHelperDg2OrBelowTests = ::testing::Test;

using isDG2OrBelow = IsAtMostProduct<IGFX_DG2>;
HWTEST2_F(HwHelperDg2OrBelowTests, WhenGettingIsKmdMigrationSupportedThenFalseIsReturned, isDG2OrBelow) {
    auto &hwHelper = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
    EXPECT_FALSE(hwHelper.isKmdMigrationSupported(*defaultHwInfo));
}