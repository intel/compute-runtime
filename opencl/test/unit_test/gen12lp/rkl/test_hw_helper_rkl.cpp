/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"

#include "opencl/test/unit_test/helpers/hw_helper_tests.h"

using HwHelperTestRkl = HwHelperTest;

RKLTEST_F(HwHelperTestRkl, givenRklA0WhenAdjustDefaultEngineTypeCalledThenRcsIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);
    hardwareInfo.featureTable.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, hardwareInfo);

    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

RKLTEST_F(HwHelperTestRkl, givenRklBWhenAdjustDefaultEngineTypeCalledThenCcsIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);
    hardwareInfo.featureTable.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hardwareInfo);

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
    const auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);
    uint32_t steppings[] = {
        REVISION_A0,
        REVISION_B};

    for (auto stepping : steppings) {
        hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(stepping, hardwareInfo);
        auto &helper = HwHelper::get(renderCoreFamily);
        EXPECT_TRUE(helper.isForceEmuInt32DivRemSPWARequired(hardwareInfo));
    }
}

RKLTEST_F(HwHelperTestRkl, givenRklWhenSteppingC0ThenIntegerDivisionEmulationIsNotEnabled) {
    const auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);
    hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_C, hardwareInfo);
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_FALSE(helper.isForceEmuInt32DivRemSPWARequired(hardwareInfo));
}
