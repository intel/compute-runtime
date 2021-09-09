/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/utilities/compiler_support.h"

#include "opencl/test/unit_test/helpers/hw_helper_tests.h"

using HwHelperTestGen12Lp = HwHelperTest;

TGLLPTEST_F(HwHelperTestGen12Lp, givenTgllpA0WhenAdjustDefaultEngineTypeCalledThenRcsIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);
    hardwareInfo.featureTable.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, hardwareInfo);

    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

TGLLPTEST_F(HwHelperTestGen12Lp, givenTgllpBWhenAdjustDefaultEngineTypeCalledThenRcsIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);
    hardwareInfo.featureTable.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A1, hardwareInfo);

    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

TGLLPTEST_F(HwHelperTestGen12Lp, givenTgllWhenWaForDefaultEngineIsNotAppliedThenCcsIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);
    hardwareInfo.featureTable.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, hardwareInfo);
    hardwareInfo.platform.eProductFamily = IGFX_UNKNOWN;

    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

TGLLPTEST_F(HwHelperTestGen12Lp, givenTgllpWhenSteppingBellowBThenIntegerDivisionEmulationIsEnabled) {
    const auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);
    hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, hardwareInfo);
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_TRUE(helper.isForceEmuInt32DivRemSPWARequired(hardwareInfo));
}

TGLLPTEST_F(HwHelperTestGen12Lp, givenTgllpWhenSteppingBThenIntegerDivisionEmulationIsEnabled) {
    const auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);
    hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A1, hardwareInfo);
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_FALSE(helper.isForceEmuInt32DivRemSPWARequired(hardwareInfo));
}

TGLLPTEST_F(HwHelperTestGen12Lp, givenTgllpAndVariousSteppingsWhenGettingIsWorkaroundRequiredThenCorrectValueIsReturned) {
    auto &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    const auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);
    uint32_t steppings[] = {
        REVISION_A0,
        REVISION_B,
        REVISION_C,
        CommonConstants::invalidStepping};

    for (auto stepping : steppings) {
        hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(stepping, hardwareInfo);

        switch (stepping) {
        case REVISION_A0:
            EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo));
            CPP_ATTRIBUTE_FALLTHROUGH;
        case REVISION_B:
            EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo));
            CPP_ATTRIBUTE_FALLTHROUGH;
        default:
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_D, hardwareInfo));
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_A0, hardwareInfo));
        }
    }
}
