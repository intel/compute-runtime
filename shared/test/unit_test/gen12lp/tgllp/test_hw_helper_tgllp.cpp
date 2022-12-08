/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_tgllp.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

using GfxCoreHelperTestGen12Lp = GfxCoreHelperTest;

TGLLPTEST_F(GfxCoreHelperTestGen12Lp, givenTgllpSteppingA0WhenAdjustDefaultEngineTypeCalledThenRcsIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &productHelper = getHelper<ProductHelper>();

    hardwareInfo.featureTable.flags.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, hardwareInfo);

    gfxCoreHelper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

TGLLPTEST_F(GfxCoreHelperTestGen12Lp, givenTgllpSteppingBWhenAdjustDefaultEngineTypeCalledThenRcsIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &productHelper = getHelper<ProductHelper>();

    hardwareInfo.featureTable.flags.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A1, hardwareInfo);

    gfxCoreHelper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

TGLLPTEST_F(GfxCoreHelperTestGen12Lp, givenTgllWhenWaForDefaultEngineIsNotAppliedThenCcsIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &productHelper = getHelper<ProductHelper>();

    hardwareInfo.featureTable.flags.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, hardwareInfo);
    hardwareInfo.platform.eProductFamily = IGFX_UNKNOWN;

    gfxCoreHelper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

TGLLPTEST_F(GfxCoreHelperTestGen12Lp, givenTgllpAndVariousSteppingsWhenGettingIsWorkaroundRequiredThenCorrectValueIsReturned) {
    auto &gfxCoreHelper = GfxCoreHelper::get(hardwareInfo.platform.eRenderCoreFamily);
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
            EXPECT_TRUE(gfxCoreHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo));
            [[fallthrough]];
        case REVISION_B:
            EXPECT_TRUE(gfxCoreHelper.isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo));
            [[fallthrough]];
        default:
            EXPECT_FALSE(gfxCoreHelper.isWorkaroundRequired(REVISION_A0, REVISION_D, hardwareInfo));
            EXPECT_FALSE(gfxCoreHelper.isWorkaroundRequired(REVISION_B, REVISION_A0, hardwareInfo));
        }
    }
}
