/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/compiler_support.h"

#include "opencl/test/unit_test/helpers/hw_helper_tests.h"

using HwHelperTestGen12Lp = HwHelperTest;

TGLLPTEST_F(HwHelperTestGen12Lp, givenTgllpA0WhenAdjustDefaultEngineTypeCalledThenRcsIsReturned) {
    hardwareInfo.featureTable.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = REVISION_A0;

    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

TGLLPTEST_F(HwHelperTestGen12Lp, givenTgllpBWhenAdjustDefaultEngineTypeCalledThenRcsIsReturned) {
    hardwareInfo.featureTable.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = REVISION_A0 + 1;

    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

TGLLPTEST_F(HwHelperTestGen12Lp, givenTgllWhenWaForDefaultEngineIsNotAppliedThenCcsIsReturned) {
    hardwareInfo.featureTable.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = REVISION_A0;
    hardwareInfo.platform.eProductFamily = IGFX_UNKNOWN;

    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

TGLLPTEST_F(HwHelperTestGen12Lp, givenTgllpWhenSteppingBellowBThenIntegerDivisionEmulationIsEnabled) {
    hardwareInfo.platform.usRevId = REVISION_A0;
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_TRUE(helper.isForceEmuInt32DivRemSPWARequired(hardwareInfo));
}

TGLLPTEST_F(HwHelperTestGen12Lp, givenTgllpWhenSteppingBThenIntegerDivisionEmulationIsEnabled) {
    hardwareInfo.platform.usRevId = REVISION_A0 + 1;
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_FALSE(helper.isForceEmuInt32DivRemSPWARequired(hardwareInfo));
}

TGLLPTEST_F(HwHelperTestGen12Lp, givenTgllpAndVariousSteppingsWhenGettingIsWorkaroundRequiredThenCorrectValueIsReturned) {
    HwHelper &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    uint32_t steppings[] = {
        REVISION_A0,
        REVISION_B,
        REVISION_C,
        CommonConstants::invalidStepping};

    for (auto stepping : steppings) {
        hardwareInfo.platform.usRevId = hwHelper.getHwRevIdFromStepping(stepping, hardwareInfo);

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
