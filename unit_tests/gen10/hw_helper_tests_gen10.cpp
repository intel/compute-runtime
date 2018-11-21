/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/helpers/hw_helper_tests.h"

typedef HwHelperTest HwHelperTestCnl;

GEN10TEST_F(HwHelperTestCnl, getMaxBarriersPerSliceReturnsCorrectSize) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(32u, helper.getMaxBarrierRegisterPerSlice());
}

GEN10TEST_F(HwHelperTestCnl, whenCnlRevIdForStepCThenSetCapabilityCoherencyFlagFalse) {
    auto &helper = HwHelper::get(renderCoreFamily);

    bool coherency = true;
    HardwareInfo testHwInfo = hwInfoHelper.hwInfo;
    PLATFORM testPlatform = *hwInfoHelper.hwInfo.pPlatform;
    testHwInfo.pPlatform = &testPlatform;
    testPlatform.usRevId = 0x3;
    helper.setCapabilityCoherencyFlag(&testHwInfo, coherency);
    EXPECT_FALSE(coherency);
}

GEN10TEST_F(HwHelperTestCnl, whenCnlRevIdForStepDThenSetCapabilityCoherencyFlagTrue) {
    auto &helper = HwHelper::get(renderCoreFamily);

    bool coherency = false;
    HardwareInfo testHwInfo = hwInfoHelper.hwInfo;
    PLATFORM testPlatform = *hwInfoHelper.hwInfo.pPlatform;
    testHwInfo.pPlatform = &testPlatform;
    testPlatform.usRevId = 0x4;
    helper.setCapabilityCoherencyFlag(&testHwInfo, coherency);
    EXPECT_TRUE(coherency);
}

GEN10TEST_F(HwHelperTestCnl, setupPreemptionRegisters) {
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

GEN10TEST_F(HwHelperTestCnl, adjustDefaultEngineType) {
    auto engineType = hwInfoHelper.hwInfo.capabilityTable.defaultEngineType;
    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hwInfoHelper.hwInfo);
    EXPECT_EQ(engineType, hwInfoHelper.hwInfo.capabilityTable.defaultEngineType);
}

GEN10TEST_F(HwHelperTestCnl, givenGen10PlatformWhenSetupHardwareCapabilitiesIsCalledThenDefaultImplementationIsUsed) {
    auto &helper = HwHelper::get(renderCoreFamily);

    // Test default method implementation
    testDefaultImplementationOfSetupHardwareCapabilities(helper, hwInfoHelper.hwInfo);
}

GEN10TEST_F(HwHelperTestCnl, whenGetConfigureAddressSpaceModeThenReturnZero) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(0u, helper.getConfigureAddressSpaceMode());
}
