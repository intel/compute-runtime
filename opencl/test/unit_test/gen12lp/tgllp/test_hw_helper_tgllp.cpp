/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/helpers/hw_helper_tests.h"

using HwHelperTestGen12Lp = HwHelperTest;

TGLLPTEST_F(HwHelperTestGen12Lp, givenTgllpA0WhenAdjustDefaultEngineTypeCalledThenRcsIsReturned) {
    hardwareInfo.featureTable.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = REVISION_A0;

    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

TGLLPTEST_F(HwHelperTestGen12Lp, givenTgllpBWhenAdjustDefaultEngineTypeCalledThenCcsIsReturned) {
    hardwareInfo.featureTable.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = REVISION_B;

    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_CCS, hardwareInfo.capabilityTable.defaultEngineType);
}

TGLLPTEST_F(HwHelperTestGen12Lp, givenTgllpWhenSteppingBellowBThenIntegerDivisionEmulationIsEnabled) {
    hardwareInfo.platform.usRevId = REVISION_A0;
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_TRUE(helper.isForceEmuInt32DivRemSPWARequired(hardwareInfo));
}

TGLLPTEST_F(HwHelperTestGen12Lp, givenTgllpWhenSteppingBThenIntegerDivisionEmulationIsEnabled) {
    hardwareInfo.platform.usRevId = REVISION_B;
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_FALSE(helper.isForceEmuInt32DivRemSPWARequired(hardwareInfo));
}
