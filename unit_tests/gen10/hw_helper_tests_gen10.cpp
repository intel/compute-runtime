/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/helpers/get_gpgpu_engines_tests.inl"
#include "unit_tests/helpers/hw_helper_tests.h"

using HwHelperTestGen10 = HwHelperTest;

GEN10TEST_F(HwHelperTestGen10, getMaxBarriersPerSliceReturnsCorrectSize) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(32u, helper.getMaxBarrierRegisterPerSlice());
}

GEN10TEST_F(HwHelperTestGen10, whenCnlRevIdForStepCThenSetCapabilityCoherencyFlagFalse) {
    auto &helper = HwHelper::get(renderCoreFamily);

    bool coherency = true;
    HardwareInfo testHwInfo = hwInfoHelper.hwInfo;
    PLATFORM testPlatform = *hwInfoHelper.hwInfo.pPlatform;
    testHwInfo.pPlatform = &testPlatform;
    testPlatform.usRevId = 0x3;
    helper.setCapabilityCoherencyFlag(&testHwInfo, coherency);
    EXPECT_FALSE(coherency);
}

GEN10TEST_F(HwHelperTestGen10, whenCnlRevIdForStepDThenSetCapabilityCoherencyFlagTrue) {
    auto &helper = HwHelper::get(renderCoreFamily);

    bool coherency = false;
    HardwareInfo testHwInfo = hwInfoHelper.hwInfo;
    PLATFORM testPlatform = *hwInfoHelper.hwInfo.pPlatform;
    testHwInfo.pPlatform = &testPlatform;
    testPlatform.usRevId = 0x4;
    helper.setCapabilityCoherencyFlag(&testHwInfo, coherency);
    EXPECT_TRUE(coherency);
}

GEN10TEST_F(HwHelperTestGen10, setupPreemptionRegisters) {
    auto &helper = HwHelper::get(renderCoreFamily);

    bool preemption = false;
    preemption = helper.setupPreemptionRegisters(&hwInfoHelper.hwInfo, preemption);
    EXPECT_FALSE(preemption);
    EXPECT_FALSE(hwInfoHelper.hwInfo.capabilityTable.whitelistedRegisters.csChicken1_0x2580);

    preemption = true;
    preemption = helper.setupPreemptionRegisters(&hwInfoHelper.hwInfo, preemption);
    EXPECT_TRUE(preemption);
    EXPECT_TRUE(hwInfoHelper.hwInfo.capabilityTable.whitelistedRegisters.csChicken1_0x2580);
}

GEN10TEST_F(HwHelperTestGen10, adjustDefaultEngineType) {
    auto engineType = hwInfoHelper.hwInfo.capabilityTable.defaultEngineType;
    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hwInfoHelper.hwInfo);
    EXPECT_EQ(engineType, hwInfoHelper.hwInfo.capabilityTable.defaultEngineType);
}

GEN10TEST_F(HwHelperTestGen10, givenGen10PlatformWhenSetupHardwareCapabilitiesIsCalledThenDefaultImplementationIsUsed) {
    auto &helper = HwHelper::get(renderCoreFamily);

    // Test default method implementation
    testDefaultImplementationOfSetupHardwareCapabilities(helper, hwInfoHelper.hwInfo);
}

GEN10TEST_F(HwHelperTestGen10, whenGetConfigureAddressSpaceModeThenReturnZero) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(0u, helper.getConfigureAddressSpaceMode());
}

GEN10TEST_F(HwHelperTestGen10, whenGetGpgpuEnginesThenReturnTwoRcsEngines) {
    whenGetGpgpuEnginesThenReturnTwoRcsEngines<FamilyType>();
    EXPECT_EQ(2u, pDevice->getExecutionEnvironment()->commandStreamReceivers[0].size());
}
