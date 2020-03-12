/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "test.h"

using namespace NEO;

typedef Test<DeviceFixture> SklDeviceCaps;

SKLTEST_F(SklDeviceCaps, reportsOcl21) {
    const auto &caps = pClDevice->getDeviceInfo();
    EXPECT_STREQ("OpenCL 2.1 NEO ", caps.clVersion);
    EXPECT_STREQ("OpenCL C 2.0 ", caps.clCVersion);
}

SKLTEST_F(SklDeviceCaps, SklProfilingTimerResolution) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_EQ(83u, caps.outProfilingTimerResolution);
}

SKLTEST_F(SklDeviceCaps, givenSklDeviceWhenAskedFor32BitSupportThenFalseIsReturned) {
    const auto &caps = pClDevice->getDeviceInfo();
    const auto &sharedCaps = pDevice->getDeviceInfo();
    EXPECT_STREQ("OpenCL 2.1 NEO ", caps.clVersion);
    EXPECT_STREQ("OpenCL C 2.0 ", caps.clCVersion);

    auto memoryManager = pDevice->getMemoryManager();
    EXPECT_FALSE(memoryManager->peekForce32BitAllocations());
    EXPECT_FALSE(sharedCaps.force32BitAddressess);
}

SKLTEST_F(SklDeviceCaps, SklSvmCapabilities) {
    const auto &caps = pClDevice->getDeviceInfo();
    cl_device_svm_capabilities expectedCaps = (CL_DEVICE_SVM_COARSE_GRAIN_BUFFER |
                                               CL_DEVICE_SVM_FINE_GRAIN_BUFFER |
                                               CL_DEVICE_SVM_ATOMICS);
    EXPECT_EQ(expectedCaps, caps.svmCapabilities);
}

typedef Test<DeviceFixture> SklUsDeviceIdTest;

SKLTEST_F(SklUsDeviceIdTest, isSimulationCap) {
    unsigned short sklSimulationIds[6] = {
        ISKL_GT0_DESK_DEVICE_F0_ID,
        ISKL_GT1_DESK_DEVICE_F0_ID,
        ISKL_GT2_DESK_DEVICE_F0_ID,
        ISKL_GT3_DESK_DEVICE_F0_ID,
        ISKL_GT4_DESK_DEVICE_F0_ID,
        0, // default, non-simulation
    };
    NEO::MockDevice *mockDevice = nullptr;

    for (auto id : sklSimulationIds) {
        mockDevice = createWithUsDeviceId(id);
        ASSERT_NE(mockDevice, nullptr);

        if (id == 0)
            EXPECT_FALSE(mockDevice->isSimulation());
        else
            EXPECT_TRUE(mockDevice->isSimulation());
        delete mockDevice;
    }
}

SKLTEST_F(SklUsDeviceIdTest, GivenSKLWhenCheckftr64KBpagesThenTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftr64KBpages);
}

SKLTEST_F(SklUsDeviceIdTest, givenSklWhenCheckFtrSupportsInteger64BitAtomicsThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsInteger64BitAtomics);
}
