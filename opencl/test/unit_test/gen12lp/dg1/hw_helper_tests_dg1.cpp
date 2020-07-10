/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

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
