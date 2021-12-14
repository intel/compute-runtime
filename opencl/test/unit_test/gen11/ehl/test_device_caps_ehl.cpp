/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

using EhlTest = Test<ClDeviceFixture>;

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
