/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/helpers/hw_helper_tests.h"

using HwHelperTestRkl = HwHelperTest;

RKLTEST_F(HwHelperTestRkl, givenRklA0WhenAdjustDefaultEngineTypeCalledThenRcsIsReturned) {
    HwHelper &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    hardwareInfo.featureTable.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = hwHelper.getHwRevIdFromStepping(REVISION_A0, hardwareInfo);

    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

RKLTEST_F(HwHelperTestRkl, givenRklBWhenAdjustDefaultEngineTypeCalledThenCcsIsReturned) {
    HwHelper &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    hardwareInfo.featureTable.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = hwHelper.getHwRevIdFromStepping(REVISION_B, hardwareInfo);

    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

RKLTEST_F(HwHelperTestRkl, givenRklWhenRequestedVmeFlagsThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.supportsVme);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsVmeAvcTextureSampler);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsVmeAvcPreemption);
}

RKLTEST_F(HwHelperTestRkl, givenRklWhenSteppingA0OrB0ThenIntegerDivisionEmulationIsEnabled) {
    HwHelper &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    uint32_t steppings[] = {
        REVISION_A0,
        REVISION_B};

    for (auto stepping : steppings) {
        hardwareInfo.platform.usRevId = hwHelper.getHwRevIdFromStepping(stepping, hardwareInfo);
        auto &helper = HwHelper::get(renderCoreFamily);
        EXPECT_TRUE(helper.isForceEmuInt32DivRemSPWARequired(hardwareInfo));
    }
}

RKLTEST_F(HwHelperTestRkl, givenRklWhenSteppingC0ThenIntegerDivisionEmulationIsNotEnabled) {
    HwHelper &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    hardwareInfo.platform.usRevId = hwHelper.getHwRevIdFromStepping(REVISION_C, hardwareInfo);
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_FALSE(helper.isForceEmuInt32DivRemSPWARequired(hardwareInfo));
}
