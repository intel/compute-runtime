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
    helper.setupHardwareCapabilities(&hwCaps);

    EXPECT_EQ(2048u, hwCaps.image3DMaxHeight);
    EXPECT_EQ(2048u, hwCaps.image3DMaxWidth);
    EXPECT_EQ(2 * MemoryConstants::gigaByte - 8 * MemoryConstants::megaByte, hwCaps.maxMemAllocSize);
}
