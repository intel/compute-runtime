/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;
using HwHelperXeHpCoreTest = ::testing::Test;

XE_HP_CORE_TEST_F(HwHelperXeHpCoreTest, givenSteppingAorBWhenCheckingSipWAThenTrueIsReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto renderCoreFamily = defaultHwInfo->platform.eRenderCoreFamily;
    auto productFamily = defaultHwInfo->platform.eProductFamily;

    auto &helper = HwHelper::get(renderCoreFamily);
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);

    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, hwInfo);
    EXPECT_TRUE(helper.isSipWANeeded(hwInfo));

    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);
    EXPECT_TRUE(helper.isSipWANeeded(hwInfo));
}

XE_HP_CORE_TEST_F(HwHelperXeHpCoreTest, givenSteppingCWhenCheckingSipWAThenFalseIsReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto renderCoreFamily = defaultHwInfo->platform.eRenderCoreFamily;
    auto productFamily = defaultHwInfo->platform.eProductFamily;

    auto &helper = HwHelper::get(renderCoreFamily);
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);

    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_C, hwInfo);
    EXPECT_FALSE(helper.isSipWANeeded(hwInfo));
}
