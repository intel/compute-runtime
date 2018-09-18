/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/helpers/hw_helper_tests.h"

typedef HwHelperTest HwHelperTestSkl;

GEN9TEST_F(HwHelperTestSkl, getMaxBarriersPerSliceReturnsCorrectSize) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(32u, helper.getMaxBarrierRegisterPerSlice());
}

GEN9TEST_F(HwHelperTestSkl, setCapabilityCoherencyFlag) {
    auto &helper = HwHelper::get(renderCoreFamily);

    bool coherency = false;
    helper.setCapabilityCoherencyFlag(&hwInfo, coherency);
    EXPECT_TRUE(coherency);
}

GEN9TEST_F(HwHelperTestSkl, setupPreemptionRegisters) {
    auto &helper = HwHelper::get(renderCoreFamily);

    bool preemption = false;
    preemption = helper.setupPreemptionRegisters(&hwInfo, preemption);
    EXPECT_FALSE(preemption);
    EXPECT_FALSE(hwInfo.capabilityTable.whitelistedRegisters.csChicken1_0x2580);

    preemption = true;
    preemption = helper.setupPreemptionRegisters(&hwInfo, preemption);
    EXPECT_TRUE(preemption);
    EXPECT_TRUE(hwInfo.capabilityTable.whitelistedRegisters.csChicken1_0x2580);
}

GEN9TEST_F(HwHelperTestSkl, adjustDefaultEngineType) {
    auto engineType = hwInfo.capabilityTable.defaultEngineType;
    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hwInfo);
    EXPECT_EQ(engineType, hwInfo.capabilityTable.defaultEngineType);
}

GEN9TEST_F(HwHelperTestSkl, givenGen9PlatformWhenSetupHardwareCapabilitiesIsCalledThenDefaultImplementationIsUsed) {
    auto &helper = HwHelper::get(renderCoreFamily);

    // Test default method implementation
    testDefaultImplementationOfSetupHardwareCapabilities(helper, hwInfo);
}

GEN9TEST_F(HwHelperTestSkl, givenDebuggingActiveWhenSipKernelTypeIsQueriedThenDbgCsrLocalTypeIsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);

    auto sipType = helper.getSipKernelType(true);
    EXPECT_EQ(SipKernelType::DbgCsrLocal, sipType);
}

GEN9TEST_F(HwHelperTestSkl, whenGetConfigureAddressSpaceModeThenReturnZero) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(0u, helper.getConfigureAddressSpaceMode());
}