/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_rkl.h"
#include "shared/source/gen12lp/hw_info_rkl.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

using GfxCoreHelperTestRkl = GfxCoreHelperTest;

RKLTEST_F(GfxCoreHelperTestRkl, givenRklSteppingA0WhenAdjustDefaultEngineTypeCalledThenRcsIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &productHelper = getHelper<ProductHelper>();

    hardwareInfo.featureTable.flags.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, hardwareInfo);

    gfxCoreHelper.adjustDefaultEngineType(&hardwareInfo, productHelper, nullptr);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

RKLTEST_F(GfxCoreHelperTestRkl, givenRklSteppingBWhenAdjustDefaultEngineTypeCalledThenRcsIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &productHelper = getHelper<ProductHelper>();

    hardwareInfo.featureTable.flags.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hardwareInfo);

    gfxCoreHelper.adjustDefaultEngineType(&hardwareInfo, productHelper, nullptr);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

RKLTEST_F(GfxCoreHelperTestRkl, givenRevisionEnumThenProperValueForIsWorkaroundRequiredIsReturned) {
    using us = unsigned short;
    constexpr us a0 = 0x0;
    constexpr us b0 = 0x4;
    constexpr us undefined = 0x5;
    us steppings[] = {a0, b0, undefined};
    HardwareInfo hardwareInfo = *defaultHwInfo;

    for (auto stepping : steppings) {
        hardwareInfo.platform.usRevId = stepping;
        auto &productHelper = getHelper<ProductHelper>();

        if (stepping == a0) {
            EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo, productHelper));
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_B, REVISION_A0, hardwareInfo, productHelper));
        } else if (stepping == b0 || stepping == undefined) {
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_D, hardwareInfo, productHelper));
        }
    }
}
