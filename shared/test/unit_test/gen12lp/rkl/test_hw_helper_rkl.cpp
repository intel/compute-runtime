/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_rkl.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

using GfxCoreHelperTestRkl = GfxCoreHelperTest;

RKLTEST_F(GfxCoreHelperTestRkl, givenRklSteppingA0WhenAdjustDefaultEngineTypeCalledThenRcsIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &productHelper = getHelper<ProductHelper>();

    hardwareInfo.featureTable.flags.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, hardwareInfo);

    gfxCoreHelper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

RKLTEST_F(GfxCoreHelperTestRkl, givenRklSteppingBWhenAdjustDefaultEngineTypeCalledThenRcsIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &productHelper = getHelper<ProductHelper>();

    hardwareInfo.featureTable.flags.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hardwareInfo);

    gfxCoreHelper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

RKLTEST_F(GfxCoreHelperTestRkl, givenRklWhenRequestedVmeFlagsThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.supportsVme);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsVmeAvcTextureSampler);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsVmeAvcPreemption);
}
