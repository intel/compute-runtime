/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
    HardwareInfo testHwInfo = hwInfo;
    PLATFORM testPlatform = *hwInfo.pPlatform;
    testHwInfo.pPlatform = &testPlatform;
    testPlatform.usRevId = 0x3;
    helper.setCapabilityCoherencyFlag(&testHwInfo, coherency);
    EXPECT_FALSE(coherency);
}

GEN10TEST_F(HwHelperTestCnl, whenCnlRevIdForStepDThenSetCapabilityCoherencyFlagTrue) {
    auto &helper = HwHelper::get(renderCoreFamily);

    bool coherency = false;
    HardwareInfo testHwInfo = hwInfo;
    PLATFORM testPlatform = *hwInfo.pPlatform;
    testHwInfo.pPlatform = &testPlatform;
    testPlatform.usRevId = 0x4;
    helper.setCapabilityCoherencyFlag(&testHwInfo, coherency);
    EXPECT_TRUE(coherency);
}

GEN10TEST_F(HwHelperTestCnl, setupPreemptionRegisters) {
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

GEN10TEST_F(HwHelperTestCnl, adjustDefaultEngineType) {
    auto engineType = hwInfo.capabilityTable.defaultEngineType;
    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hwInfo);
    EXPECT_EQ(engineType, hwInfo.capabilityTable.defaultEngineType);
}

GEN10TEST_F(HwHelperTestCnl, givenGen10PlatformWhenSetupHardwareCapabilitiesIsCalledThenDefaultImplementationIsUsed) {
    auto &helper = HwHelper::get(renderCoreFamily);

    // Test default method implementation
    testDefaultImplementationOfSetupHardwareCapabilities(helper, hwInfo);
}

GEN10TEST_F(HwHelperTestCnl, whenGetConfigureAddressSpaceModeThenReturnZero) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(0u, helper.getConfigureAddressSpaceMode());
}
