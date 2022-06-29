/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen8/hw_cmds_bdw.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;
using BdwUsDeviceIdTest = ::testing::Test;

BDWTEST_F(BdwUsDeviceIdTest, WhenCheckingIsSimulationThenTrueReturnedOnlyForSimulationId) {
    unsigned short bdwSimulationIds[6] = {
        0x0BD0,
        0x0BD1,
        0x0BD2,
        0x0BD3,
        0x0BD4,
        0, // default, non-simulation
    };

    for (auto &id : bdwSimulationIds) {
        auto hardwareInfo = *defaultHwInfo;
        hardwareInfo.platform.usDeviceID = id;
        std::unique_ptr<MockDevice> mockDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo, 0u /*rootDeviceIndex*/));
        ASSERT_NE(mockDevice.get(), nullptr);

        if (id == 0)
            EXPECT_FALSE(mockDevice->isSimulation());
        else
            EXPECT_TRUE(mockDevice->isSimulation());
    }
}
