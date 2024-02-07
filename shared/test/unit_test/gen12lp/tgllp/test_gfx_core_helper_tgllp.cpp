/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_tgllp.h"
#include "shared/source/gen12lp/hw_info_tgllp.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

using GfxCoreHelperTestGen12Lp = GfxCoreHelperTest;

TGLLPTEST_F(GfxCoreHelperTestGen12Lp, givenTgllpSteppingA0WhenAdjustDefaultEngineTypeCalledThenRcsIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &productHelper = getHelper<ProductHelper>();

    hardwareInfo.featureTable.flags.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, hardwareInfo);

    gfxCoreHelper.adjustDefaultEngineType(&hardwareInfo, productHelper, nullptr);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

TGLLPTEST_F(GfxCoreHelperTestGen12Lp, givenTgllpSteppingBWhenAdjustDefaultEngineTypeCalledThenRcsIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &productHelper = getHelper<ProductHelper>();

    hardwareInfo.featureTable.flags.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A1, hardwareInfo);

    gfxCoreHelper.adjustDefaultEngineType(&hardwareInfo, productHelper, nullptr);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

TGLLPTEST_F(GfxCoreHelperTestGen12Lp, givenTgllWhenWaForDefaultEngineIsNotAppliedThenCcsIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &productHelper = getHelper<ProductHelper>();

    hardwareInfo.featureTable.flags.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, hardwareInfo);
    hardwareInfo.platform.eProductFamily = IGFX_UNKNOWN;

    gfxCoreHelper.adjustDefaultEngineType(&hardwareInfo, productHelper, nullptr);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

TGLLPTEST_F(GfxCoreHelperTestGen12Lp, givenTgllpAndVariousSteppingsWhenGettingIsWorkaroundRequiredThenCorrectValueIsReturned) {

    const auto &productHelper = getHelper<ProductHelper>();
    uint32_t steppings[] = {
        REVISION_A0,
        REVISION_B,
        REVISION_C,
        CommonConstants::invalidStepping};

    for (auto stepping : steppings) {
        hardwareInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(stepping, hardwareInfo);

        switch (stepping) {
        case REVISION_A0:
            EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo, productHelper));
            [[fallthrough]];
        case REVISION_B:
            EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo, productHelper));
            [[fallthrough]];
        default:
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_D, hardwareInfo, productHelper));
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_B, REVISION_A0, hardwareInfo, productHelper));
        }
    }
}
