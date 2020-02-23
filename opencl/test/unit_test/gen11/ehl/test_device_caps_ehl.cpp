/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "test.h"

using namespace NEO;

using EhlTest = Test<DeviceFixture>;

EHLTEST_F(EhlTest, givenDeviceIdWhenAskingForSimulationThenReturnValidValue) {
    unsigned short ehlSimulationIds[2] = {
        IEHL_1x4x8_SUPERSKU_DEVICE_A0_ID,
        0, // default, non-simulation
    };

    for (auto id : ehlSimulationIds) {
        auto mockDevice = std::unique_ptr<MockDevice>(createWithUsDeviceId(id));
        EXPECT_NE(nullptr, mockDevice);

        if (id == 0) {
            EXPECT_FALSE(mockDevice->isSimulation());
        } else {
            EXPECT_TRUE(mockDevice->isSimulation());
        }
    }
}

EHLTEST_F(EhlTest, givenEhlWhenSlmSizeIsRequiredThenReturnCorrectValue) {
    EXPECT_EQ(64u, pDevice->getHardwareInfo().capabilityTable.slmSize);
}
