/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"

using namespace NEO;

typedef Test<DeviceFixture> BxtDeviceCaps;

BXTTEST_F(BxtDeviceCaps, reportsOcl12) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_STREQ("OpenCL 1.2 NEO ", caps.clVersion);
    EXPECT_STREQ("OpenCL C 1.2 ", caps.clCVersion);
}

BXTTEST_F(BxtDeviceCaps, BxtProfilingTimerResolution) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_EQ(52u, caps.outProfilingTimerResolution);
}

BXTTEST_F(BxtDeviceCaps, BxtClVersionSupport) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_STREQ("OpenCL 1.2 NEO ", caps.clVersion);
    EXPECT_STREQ("OpenCL C 1.2 ", caps.clCVersion);

    auto memoryManager = pDevice->getMemoryManager();
    if (is64bit) {
        EXPECT_TRUE(memoryManager->peekForce32BitAllocations());
        EXPECT_TRUE(caps.force32BitAddressess);
    } else {
        EXPECT_FALSE(memoryManager->peekForce32BitAllocations());
        EXPECT_FALSE(caps.force32BitAddressess);
    }
}

BXTTEST_F(BxtDeviceCaps, BxtSvmCapabilities) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_EQ(0u, caps.svmCapabilities);
}

BXTTEST_F(BxtDeviceCaps, GivenBXTWhenCheckftr64KBpagesThenFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftr64KBpages);
}

BXTTEST_F(BxtDeviceCaps, givenBXTWhenCheckFtrSupportsInteger64BitAtomicsThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsInteger64BitAtomics);
}

typedef Test<DeviceFixture> BxtUsDeviceIdTest;

BXTTEST_F(BxtUsDeviceIdTest, isSimulationCap) {
    unsigned short bxtSimulationIds[3] = {
        IBXT_A_DEVICE_F0_ID,
        IBXT_C_DEVICE_F0_ID,
        0, // default, non-simulation
    };
    NEO::MockDevice *mockDevice = nullptr;

    for (auto id : bxtSimulationIds) {
        mockDevice = createWithUsDeviceId(id);
        ASSERT_NE(mockDevice, nullptr);

        if (id == 0)
            EXPECT_FALSE(mockDevice->isSimulation());
        else
            EXPECT_TRUE(mockDevice->isSimulation());
        delete mockDevice;
    }
}
