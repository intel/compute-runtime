/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_dg1.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

#include "opencl/source/helpers/cl_hw_helper.h"

using HwHelperTestDg1 = HwHelperTest;

DG1TEST_F(HwHelperTestDg1, givenDg1SteppingA0WhenAdjustDefaultEngineTypeCalledThenRcsIsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    hardwareInfo.featureTable.flags.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, hardwareInfo);

    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

DG1TEST_F(HwHelperTestDg1, givenDg1SteppingBWhenAdjustDefaultEngineTypeCalledThenRcsIsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    hardwareInfo.featureTable.flags.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hardwareInfo);

    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

DG1TEST_F(HwHelperTestDg1, givenDg1AndVariousSteppingsWhenGettingIsWorkaroundRequiredThenCorrectValueIsReturned) {
    const auto &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    const auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);
    uint32_t steppings[] = {
        REVISION_A0,
        REVISION_B,
        CommonConstants::invalidStepping};

    for (auto stepping : steppings) {
        hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(stepping, hardwareInfo);

        switch (stepping) {
        case REVISION_A0:
            EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo));
            [[fallthrough]];
        default:
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_A0, hardwareInfo));
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_D, hardwareInfo));
        }
    }
}

DG1TEST_F(HwHelperTestDg1, givenBufferAllocationTypeWhenSetExtraAllocationDataIsCalledThenIsLockableIsSet) {
    auto &hwHelper = HwHelper::get(renderCoreFamily);
    AllocationData allocData{};
    allocData.flags.useSystemMemory = true;
    AllocationProperties allocProperties(0, 1, AllocationType::BUFFER, {});
    allocData.storageInfo.isLockable = false;
    allocProperties.flags.shareable = false;
    hwHelper.setExtraAllocationData(allocData, allocProperties, *defaultHwInfo);
    EXPECT_TRUE(allocData.storageInfo.isLockable);
}

DG1TEST_F(HwHelperTestDg1, givenBufferAllocationTypeWhenSetExtraAllocationDataIsCalledWithShareableSetThenIsLockableIsFalse) {
    auto &hwHelper = HwHelper::get(renderCoreFamily);
    AllocationData allocData{};
    allocData.flags.useSystemMemory = true;
    AllocationProperties allocProperties(0, 1, AllocationType::BUFFER, {});
    allocData.storageInfo.isLockable = false;
    allocProperties.flags.shareable = true;
    hwHelper.setExtraAllocationData(allocData, allocProperties, *defaultHwInfo);
    EXPECT_FALSE(allocData.storageInfo.isLockable);
}
