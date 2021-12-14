/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

using AdlsUsDeviceIdTest = Test<ClDeviceFixture>;

ADLSTEST_F(AdlsUsDeviceIdTest, WhenCheckingIsSimulationThenTrueReturnedOnlyForSimulationId) {
    unsigned short adlsSimulationIds[1] = {
        0, // default, non-simulation
    };
    NEO::MockDevice *mockDevice = nullptr;

    for (auto id : adlsSimulationIds) {
        mockDevice = createWithUsDeviceId(id);
        ASSERT_NE(mockDevice, nullptr);

        EXPECT_FALSE(mockDevice->isSimulation());
        delete mockDevice;
    }
}

ADLSTEST_F(AdlsUsDeviceIdTest, givenAdlsWhenCheckFtrSupportsInteger64BitAtomicsThenReturnFalse) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsInteger64BitAtomics);
}

ADLSTEST_F(AdlsUsDeviceIdTest, givenAdlsWhenRequestedVmeFlagsThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.supportsVme);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsVmeAvcTextureSampler);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsVmeAvcPreemption);
}
