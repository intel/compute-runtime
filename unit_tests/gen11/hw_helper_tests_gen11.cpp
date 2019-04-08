/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/helpers/get_gpgpu_engines_tests.inl"
#include "unit_tests/helpers/hw_helper_tests.h"

using HwHelperTestGen11 = HwHelperTest;

GEN11TEST_F(HwHelperTestGen11, getMaxBarriersPerSliceReturnsCorrectSize) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(32u, helper.getMaxBarrierRegisterPerSlice());
}

GEN11TEST_F(HwHelperTestGen11, setCapabilityCoherencyFlag) {
    auto &helper = HwHelper::get(renderCoreFamily);

    bool coherency = false;
    helper.setCapabilityCoherencyFlag(&hwInfoHelper.hwInfo, coherency);
    EXPECT_TRUE(coherency);
}

GEN11TEST_F(HwHelperTestGen11, setupPreemptionRegisters) {
    auto &helper = HwHelper::get(renderCoreFamily);

    bool preemption = false;
    preemption = helper.setupPreemptionRegisters(&hwInfoHelper.hwInfo, preemption);
    EXPECT_FALSE(preemption);

    preemption = true;
    preemption = helper.setupPreemptionRegisters(&hwInfoHelper.hwInfo, preemption);
    EXPECT_TRUE(preemption);
}

GEN11TEST_F(HwHelperTestGen11, adjustDefaultEngineType) {
    auto engineType = hwInfoHelper.hwInfo.capabilityTable.defaultEngineType;
    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hwInfoHelper.hwInfo);
    EXPECT_EQ(engineType, hwInfoHelper.hwInfo.capabilityTable.defaultEngineType);
}

GEN11TEST_F(HwHelperTestGen11, givenGen11PlatformWhenSetupHardwareCapabilitiesIsCalledThenDefaultImplementationIsUsed) {
    auto &helper = HwHelper::get(renderCoreFamily);

    // Test default method implementation
    testDefaultImplementationOfSetupHardwareCapabilities(helper, hwInfoHelper.hwInfo);
}

GEN11TEST_F(HwHelperTestGen11, whenGetConfigureAddressSpaceModeThenReturnZero) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(0u, helper.getConfigureAddressSpaceMode());
}

GEN11TEST_F(HwHelperTestGen11, whenGetGpgpuEnginesThenReturnTwoRcsEngines) {
    whenGetGpgpuEnginesThenReturnTwoRcsEngines<FamilyType>();
    EXPECT_EQ(2u, pDevice->getExecutionEnvironment()->commandStreamReceivers[0].size());
}
