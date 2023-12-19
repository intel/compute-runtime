/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

using SysmanProductHelperFrequencyTest = ::testing::Test;
using IsProductXeHpSdvPlus = IsAtLeastProduct<IGFX_XE_HP_SDV>;

HWTEST2_F(SysmanProductHelperFrequencyTest, GivenFrequencyModuleWhenGettingStepSizeThenValidStepSizeIsReturned, IsProductXeHpSdvPlus) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double stepSize = 0;
    pSysmanProductHelper->getFrequencyStepSize(&stepSize);
    EXPECT_EQ(50.0, stepSize);
}

HWTEST2_F(SysmanProductHelperFrequencyTest, GivenFrequencyModuleWhenGettingStepSizeThenValidStepSizeIsReturned, IsGen9) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double stepSize = 0;
    pSysmanProductHelper->getFrequencyStepSize(&stepSize);
    EXPECT_EQ((50.0 / 3), stepSize);
}

} // namespace ult
} // namespace Sysman
} // namespace L0