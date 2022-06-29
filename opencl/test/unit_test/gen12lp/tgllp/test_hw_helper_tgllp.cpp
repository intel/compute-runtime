/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_tgllp.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

using HwHelperTestGen12Lp = HwHelperTest;

TGLLPTEST_F(HwHelperTestGen12Lp, givenTgllpSteppingA0WhenAdjustDefaultEngineTypeCalledThenRcsIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);
    hardwareInfo.featureTable.flags.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, hardwareInfo);

    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

TGLLPTEST_F(HwHelperTestGen12Lp, givenTgllpSteppingBWhenAdjustDefaultEngineTypeCalledThenRcsIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);
    hardwareInfo.featureTable.flags.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A1, hardwareInfo);

    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

TGLLPTEST_F(HwHelperTestGen12Lp, givenTgllWhenWaForDefaultEngineIsNotAppliedThenCcsIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);
    hardwareInfo.featureTable.flags.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, hardwareInfo);
    hardwareInfo.platform.eProductFamily = IGFX_UNKNOWN;

    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
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
            [[fallthrough]];
        case REVISION_B:
            EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo));
            [[fallthrough]];
        default:
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_D, hardwareInfo));
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_A0, hardwareInfo));
        }
    }
}
