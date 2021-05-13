/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/compiler_support.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/test/unit_test/helpers/hw_helper_tests.h"

using HwHelperTestDg1 = HwHelperTest;

DG1TEST_F(HwHelperTestDg1, givenDg1PlatformWhenIsLocalMemoryEnabledIsCalledThenTrueIsReturned) {
    hardwareInfo.featureTable.ftrLocalMemory = true;

    auto &helper = reinterpret_cast<HwHelperHw<FamilyType> &>(HwHelperHw<FamilyType>::get());
    EXPECT_TRUE(helper.isLocalMemoryEnabled(hardwareInfo));
}

DG1TEST_F(HwHelperTestDg1, givenDg1PlatformWithoutLocalMemoryFeatureWhenIsLocalMemoryEnabledIsCalledThenFalseIsReturned) {
    hardwareInfo.featureTable.ftrLocalMemory = false;

    auto &helper = reinterpret_cast<HwHelperHw<FamilyType> &>(HwHelperHw<FamilyType>::get());
    EXPECT_FALSE(helper.isLocalMemoryEnabled(hardwareInfo));
}

DG1TEST_F(HwHelperTestDg1, givenDg1PlatformWhenSetupHardwareCapabilitiesIsCalledThenThenSpecificImplementationIsUsed) {
    hardwareInfo.featureTable.ftrLocalMemory = true;

    HardwareCapabilities hwCaps = {0};
    auto &helper = HwHelper::get(renderCoreFamily);
    helper.setupHardwareCapabilities(&hwCaps, hardwareInfo);

    EXPECT_EQ(2048u, hwCaps.image3DMaxHeight);
    EXPECT_EQ(2048u, hwCaps.image3DMaxWidth);
    EXPECT_TRUE(hwCaps.isStatelesToStatefullWithOffsetSupported);
}

DG1TEST_F(HwHelperTestDg1, givenDg1A0WhenAdjustDefaultEngineTypeCalledThenRcsIsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    hardwareInfo.featureTable.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = helper.getHwRevIdFromStepping(REVISION_A0, hardwareInfo);

    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

DG1TEST_F(HwHelperTestDg1, givenDg1BWhenAdjustDefaultEngineTypeCalledThenCcsIsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    hardwareInfo.featureTable.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = helper.getHwRevIdFromStepping(REVISION_B, hardwareInfo);

    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

DG1TEST_F(HwHelperTestDg1, givenDg1AndVariousSteppingsWhenGettingIsWorkaroundRequiredThenCorrectValueIsReturned) {
    HwHelper &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    uint32_t steppings[] = {
        REVISION_A0,
        REVISION_B,
        CommonConstants::invalidStepping};

    for (auto stepping : steppings) {
        hardwareInfo.platform.usRevId = hwHelper.getHwRevIdFromStepping(stepping, hardwareInfo);

        switch (stepping) {
        case REVISION_A0:
            EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo));
            CPP_ATTRIBUTE_FALLTHROUGH;
        default:
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_A0, hardwareInfo));
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_D, hardwareInfo));
        }
    }
}

DG1TEST_F(HwHelperTestDg1, givenDg1WhenSteppingA0ThenIntegerDivisionEmulationIsEnabled) {
    HwHelper &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    uint32_t stepping = REVISION_A0;
    hardwareInfo.platform.usRevId = hwHelper.getHwRevIdFromStepping(stepping, hardwareInfo);
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_TRUE(helper.isForceEmuInt32DivRemSPWARequired(hardwareInfo));
}

DG1TEST_F(HwHelperTestDg1, givenDg1WhenSteppingB0ThenIntegerDivisionEmulationIsNotEnabled) {
    HwHelper &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    hardwareInfo.platform.usRevId = hwHelper.getHwRevIdFromStepping(REVISION_B, hardwareInfo);
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_FALSE(helper.isForceEmuInt32DivRemSPWARequired(hardwareInfo));
}

DG1TEST_F(HwHelperTestDg1, givenDg1WhenObtainingBlitterPreferenceThenReturnFalse) {
    auto &helper = HwHelper::get(renderCoreFamily);

    EXPECT_FALSE(helper.obtainBlitterPreference(hardwareInfo));
}

DG1TEST_F(HwHelperTestDg1, givenDg1WhenGettingLocalMemoryAccessModeThenReturnCpuAccessDefault) {
    struct MockHwHelper : HwHelperHw<FamilyType> {
        using HwHelper::getDefaultLocalMemoryAccessMode;
    };

    auto hwHelper = static_cast<MockHwHelper &>(HwHelper::get(renderCoreFamily));

    EXPECT_EQ(LocalMemoryAccessMode::Default, hwHelper.getDefaultLocalMemoryAccessMode(*defaultHwInfo));
}

DG1TEST_F(HwHelperTestDg1, givenBufferAllocationTypeWhenSetExtraAllocationDataIsCalledThenIsLockableIsSet) {
    auto &hwHelper = HwHelper::get(renderCoreFamily);
    AllocationData allocData{};
    allocData.flags.useSystemMemory = true;
    AllocationProperties allocProperties(0, 1, GraphicsAllocation::AllocationType::BUFFER, {});
    allocData.storageInfo.isLockable = false;
    hwHelper.setExtraAllocationData(allocData, allocProperties, *defaultHwInfo);
    EXPECT_TRUE(allocData.storageInfo.isLockable);
}
