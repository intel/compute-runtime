/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/helpers/get_gpgpu_engines_tests.inl"
#include "opencl/test/unit_test/helpers/hw_helper_tests.h"

using HwHelperTestGen9 = HwHelperTest;

GEN9TEST_F(HwHelperTestGen9, getMaxBarriersPerSliceReturnsCorrectSize) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(32u, helper.getMaxBarrierRegisterPerSlice());
}

GEN9TEST_F(HwHelperTestGen9, setCapabilityCoherencyFlag) {
    auto &helper = HwHelper::get(renderCoreFamily);

    bool coherency = false;
    helper.setCapabilityCoherencyFlag(&hardwareInfo, coherency);
    EXPECT_TRUE(coherency);
}

GEN9TEST_F(HwHelperTestGen9, getPitchAlignmentForImage) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(4u, helper.getPitchAlignmentForImage(&hardwareInfo));
}

GEN9TEST_F(HwHelperTestGen9, adjustDefaultEngineType) {
    auto engineType = hardwareInfo.capabilityTable.defaultEngineType;
    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(engineType, hardwareInfo.capabilityTable.defaultEngineType);
}

GEN9TEST_F(HwHelperTestGen9, givenGen9PlatformWhenSetupHardwareCapabilitiesIsCalledThenDefaultImplementationIsUsed) {
    auto &helper = HwHelper::get(renderCoreFamily);

    // Test default method implementation
    testDefaultImplementationOfSetupHardwareCapabilities(helper, hardwareInfo);
}

GEN9TEST_F(HwHelperTestGen9, givenDebuggingActiveWhenSipKernelTypeIsQueriedThenDbgCsrLocalTypeIsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);

    auto sipType = helper.getSipKernelType(true);
    EXPECT_EQ(SipKernelType::DbgCsrLocal, sipType);
}

GEN9TEST_F(HwHelperTestGen9, whenGetGpgpuEnginesThenReturnThreeRcsEngines) {
    whenGetGpgpuEnginesThenReturnTwoRcsEngines<FamilyType>();
    EXPECT_EQ(3u, pDevice->engines.size());
}
