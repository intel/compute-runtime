/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/helpers/get_gpgpu_engines_tests.inl"
#include "opencl/test/unit_test/helpers/hw_helper_tests.h"

using HwHelperTestGen11 = HwHelperTest;

GEN11TEST_F(HwHelperTestGen11, getMaxBarriersPerSliceReturnsCorrectSize) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(32u, helper.getMaxBarrierRegisterPerSlice());
}

GEN11TEST_F(HwHelperTestGen11, setCapabilityCoherencyFlag) {
    auto &helper = HwHelper::get(renderCoreFamily);

    bool coherency = false;
    helper.setCapabilityCoherencyFlag(&hardwareInfo, coherency);
    EXPECT_TRUE(coherency);
}

GEN11TEST_F(HwHelperTestGen11, getPitchAlignmentForImage) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(4u, helper.getPitchAlignmentForImage(&hardwareInfo));
}

GEN11TEST_F(HwHelperTestGen11, adjustDefaultEngineType) {
    auto engineType = hardwareInfo.capabilityTable.defaultEngineType;
    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(engineType, hardwareInfo.capabilityTable.defaultEngineType);
}

GEN11TEST_F(HwHelperTestGen11, givenGen11PlatformWhenSetupHardwareCapabilitiesIsCalledThenDefaultImplementationIsUsed) {
    auto &helper = HwHelper::get(renderCoreFamily);

    // Test default method implementation
    testDefaultImplementationOfSetupHardwareCapabilities(helper, hardwareInfo);
}

GEN11TEST_F(HwHelperTestGen11, whenGetGpgpuEnginesThenReturnThreeRcsEngines) {
    whenGetGpgpuEnginesThenReturnTwoRcsEngines<FamilyType>();
    EXPECT_EQ(3u, pDevice->engines.size());
}
