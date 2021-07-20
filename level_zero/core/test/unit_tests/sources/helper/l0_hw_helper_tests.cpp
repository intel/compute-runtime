/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"

#include "test.h"

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"

namespace L0 {
namespace ult {

using L0HwHelperTest = ::testing::Test;

using PlatformsWithWa = IsWithinGfxCore<IGFX_GEN12LP_CORE, IGFX_XE_HP_CORE>;

HWTEST2_F(L0HwHelperTest, givenResumeWANotNeededThenFalseIsReturned, IsAtMostGen11) {
    auto &l0HwHelper = L0HwHelper::get(NEO::defaultHwInfo->platform.eRenderCoreFamily);

    EXPECT_FALSE(l0HwHelper.isResumeWARequired());
}

HWTEST2_F(L0HwHelperTest, givenResumeWANeededThenTrueIsReturned, PlatformsWithWa) {
    auto &l0HwHelper = L0HwHelper::get(NEO::defaultHwInfo->platform.eRenderCoreFamily);

    EXPECT_TRUE(l0HwHelper.isResumeWARequired());
}

} // namespace ult
} // namespace L0