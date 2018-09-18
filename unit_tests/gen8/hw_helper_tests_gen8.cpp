/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/helpers/hw_helper_tests.h"
#include "runtime/memory_manager/memory_constants.h"

typedef HwHelperTest HwHelperTestBdw;

GEN8TEST_F(HwHelperTestBdw, getMaxBarriersPerSliceReturnsCorrectSize) {
    auto &helper = HwHelper::get(renderCoreFamily);

    EXPECT_EQ(16u, helper.getMaxBarrierRegisterPerSlice());
}

GEN8TEST_F(HwHelperTestBdw, setCapabilityCoherencyFlag) {
    auto &helper = HwHelper::get(renderCoreFamily);

    bool coherency = false;
    helper.setCapabilityCoherencyFlag(&hwInfo, coherency);
    EXPECT_TRUE(coherency);
}

GEN8TEST_F(HwHelperTestBdw, setupPreemptionRegisters) {
    auto &helper = HwHelper::get(renderCoreFamily);

    bool preemption = false;
    preemption = helper.setupPreemptionRegisters(&hwInfo, preemption);
    EXPECT_FALSE(preemption);

    preemption = true;
    preemption = helper.setupPreemptionRegisters(&hwInfo, preemption);
    EXPECT_FALSE(preemption);
}

GEN8TEST_F(HwHelperTestBdw, adjustDefaultEngineType) {
    auto engineType = hwInfo.capabilityTable.defaultEngineType;
    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hwInfo);
    EXPECT_EQ(engineType, hwInfo.capabilityTable.defaultEngineType);
}

GEN8TEST_F(HwHelperTestBdw, givenGen8PlatformWhenSetupHardwareCapabilitiesIsCalledThenSpecificImplementationIsUsed) {
    auto &helper = HwHelper::get(renderCoreFamily);
    HardwareCapabilities hwCaps = {0};
    helper.setupHardwareCapabilities(&hwCaps, hwInfo);

    EXPECT_EQ(2048u, hwCaps.image3DMaxHeight);
    EXPECT_EQ(2048u, hwCaps.image3DMaxWidth);
    EXPECT_EQ(2 * MemoryConstants::gigaByte - 8 * MemoryConstants::megaByte, hwCaps.maxMemAllocSize);
    EXPECT_FALSE(hwCaps.isStatelesToStatefullWithOffsetSupported);
}

GEN8TEST_F(HwHelperTestBdw, whenGetConfigureAddressSpaceModeThenReturnZero) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(0u, helper.getConfigureAddressSpaceMode());
}