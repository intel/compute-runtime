/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/compiler_support.h"

#include "opencl/test/unit_test/helpers/hw_helper_tests.h"
#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"

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
    hardwareInfo.featureTable.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = REVISION_A0;

    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

DG1TEST_F(HwHelperTestDg1, givenDg1BWhenAdjustDefaultEngineTypeCalledThenCcsIsReturned) {
    hardwareInfo.featureTable.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = REVISION_B;

    auto &helper = HwHelper::get(renderCoreFamily);
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

DG1TEST_F(HwHelperTestDg1, givenDg1WhenPatchingCPUAccessibleGlobalBuffersThenDontUseBlitter) {
    uint64_t gpuAddress = 0x1000;
    void *buffer = reinterpret_cast<void *>(gpuAddress);
    size_t size = 0x1000;

    MockGraphicsAllocation mockAllocation(buffer, gpuAddress, size);
    HwHelper &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    EXPECT_FALSE(hwHelper.forceBlitterUseForGlobalBuffers(hardwareInfo, &mockAllocation));
}

DG1TEST_F(HwHelperTestDg1, givenDg1WhenPatchingCPUInaccessibleGlobalBuffersThenUseBlitter) {
    uint64_t gpuAddress = 0x1000;
    void *buffer = reinterpret_cast<void *>(0x0);
    size_t size = 0x1000;

    MockGraphicsAllocation mockAllocation(buffer, gpuAddress, size);
    HwHelper &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    EXPECT_TRUE(hwHelper.forceBlitterUseForGlobalBuffers(hardwareInfo, &mockAllocation));
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
