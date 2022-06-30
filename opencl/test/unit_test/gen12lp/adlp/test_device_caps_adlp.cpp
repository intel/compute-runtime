/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

using AdlpUsDeviceIdTest = Test<ClDeviceFixture>;

ADLPTEST_F(AdlpUsDeviceIdTest, GivenNonZeroIdThenIsSimulationIsTrue) {
    unsigned short simulationIds[] = {
        0, // default, non-simulation
    };

    for (auto id : simulationIds) {
        auto mockDevice = std::unique_ptr<MockDevice>(createWithUsDeviceId(id));
        ASSERT_NE(mockDevice.get(), nullptr);

        if (id == 0) {
            EXPECT_FALSE(mockDevice->isSimulation());
        } else {
            EXPECT_TRUE(mockDevice->isSimulation());
        }
    }
}

ADLPTEST_F(AdlpUsDeviceIdTest, givenADLPWhenCheckFtrSupportsInteger64BitAtomicsThenReturnFalse) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsInteger64BitAtomics);
}

ADLPTEST_F(AdlpUsDeviceIdTest, givenAdlpWhenRequestedVmeFlagsThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.supportsVme);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsVmeAvcTextureSampler);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsVmeAvcPreemption);
}
