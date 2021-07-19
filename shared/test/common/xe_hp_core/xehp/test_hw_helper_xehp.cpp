/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"

#include "test.h"

using namespace NEO;
using HwHelperTestXeHP = ::testing::Test;

XEHPTEST_F(HwHelperTestXeHP, givenSteppingWhenAskingForLocalMemoryAccessModeThenDisallowOnA0) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto renderCoreFamily = defaultHwInfo->platform.eRenderCoreFamily;

    auto &helper = HwHelper::get(renderCoreFamily);

    hwInfo.platform.usRevId = helper.getHwRevIdFromStepping(REVISION_A0, hwInfo);
    EXPECT_EQ(LocalMemoryAccessMode::CpuAccessDisallowed, helper.getLocalMemoryAccessMode(hwInfo));

    hwInfo.platform.usRevId = helper.getHwRevIdFromStepping(REVISION_B, hwInfo);
    EXPECT_EQ(LocalMemoryAccessMode::Default, helper.getLocalMemoryAccessMode(hwInfo));
}

XEHPTEST_F(HwHelperTestXeHP, givenSteppingAorBWhenCheckingSipWAThenTrueIsReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto renderCoreFamily = defaultHwInfo->platform.eRenderCoreFamily;

    auto &helper = HwHelper::get(renderCoreFamily);

    hwInfo.platform.usRevId = helper.getHwRevIdFromStepping(REVISION_A0, hwInfo);
    EXPECT_TRUE(helper.isSipWANeeded(hwInfo));

    hwInfo.platform.usRevId = helper.getHwRevIdFromStepping(REVISION_B, hwInfo);
    EXPECT_TRUE(helper.isSipWANeeded(hwInfo));
}

XEHPTEST_F(HwHelperTestXeHP, givenSteppingCWhenCheckingSipWAThenFalseIsReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto renderCoreFamily = defaultHwInfo->platform.eRenderCoreFamily;

    auto &helper = HwHelper::get(renderCoreFamily);

    hwInfo.platform.usRevId = helper.getHwRevIdFromStepping(REVISION_C, hwInfo);
    EXPECT_FALSE(helper.isSipWANeeded(hwInfo));
}
