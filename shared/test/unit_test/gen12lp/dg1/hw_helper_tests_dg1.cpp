/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_dg1.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

using GfxCoreHelperTestDg1 = GfxCoreHelperTest;

DG1TEST_F(GfxCoreHelperTestDg1, givenDg1SteppingA0WhenAdjustDefaultEngineTypeCalledThenRcsIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &productHelper = getHelper<ProductHelper>();
    hardwareInfo.featureTable.flags.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, hardwareInfo);

    gfxCoreHelper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

DG1TEST_F(GfxCoreHelperTestDg1, givenDg1SteppingBWhenAdjustDefaultEngineTypeCalledThenRcsIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &productHelper = getHelper<ProductHelper>();
    hardwareInfo.featureTable.flags.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hardwareInfo);

    gfxCoreHelper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

DG1TEST_F(GfxCoreHelperTestDg1, givenDg1AndVariousSteppingsWhenGettingIsWorkaroundRequiredThenCorrectValueIsReturned) {
    const auto &productHelper = getHelper<ProductHelper>();
    uint32_t steppings[] = {
        REVISION_A0,
        REVISION_B,
        CommonConstants::invalidStepping};

    for (auto stepping : steppings) {
        hardwareInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(stepping, hardwareInfo);

        switch (stepping) {
        case REVISION_A0:
            EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo, productHelper));
            [[fallthrough]];
        default:
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_B, REVISION_A0, hardwareInfo, productHelper));
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_D, hardwareInfo, productHelper));
        }
    }
}

DG1TEST_F(GfxCoreHelperTestDg1, givenBufferAllocationTypeWhenSetExtraAllocationDataIsCalledThenIsLockableIsSet) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    AllocationData allocData{};
    allocData.flags.useSystemMemory = true;
    AllocationProperties allocProperties(0, 1, AllocationType::BUFFER, {});
    allocData.storageInfo.isLockable = false;
    allocProperties.flags.shareable = false;
    gfxCoreHelper.setExtraAllocationData(allocData, allocProperties, *defaultHwInfo);
    EXPECT_TRUE(allocData.storageInfo.isLockable);
}

DG1TEST_F(GfxCoreHelperTestDg1, givenBufferAllocationTypeWhenSetExtraAllocationDataIsCalledWithShareableSetThenIsLockableIsFalse) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    AllocationData allocData{};
    allocData.flags.useSystemMemory = true;
    AllocationProperties allocProperties(0, 1, AllocationType::BUFFER, {});
    allocData.storageInfo.isLockable = false;
    allocProperties.flags.shareable = true;
    gfxCoreHelper.setExtraAllocationData(allocData, allocProperties, *defaultHwInfo);
    EXPECT_FALSE(allocData.storageInfo.isLockable);
}
