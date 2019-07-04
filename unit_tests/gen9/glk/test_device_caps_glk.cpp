/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"

using namespace NEO;

typedef Test<DeviceFixture> Gen9DeviceCaps;

GLKTEST_F(Gen9DeviceCaps, GlkProfilingTimerResolution) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_EQ(52u, caps.outProfilingTimerResolution);
}

GLKTEST_F(Gen9DeviceCaps, givenGlkDeviceWhenAskedForDoubleSupportThenTrueIsReturned) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsFP64);
}

GLKTEST_F(Gen9DeviceCaps, GlkClVersionSupport) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_STREQ("OpenCL 1.2 NEO ", caps.clVersion);
    EXPECT_STREQ("OpenCL C 1.2 ", caps.clCVersion);
}

GLKTEST_F(Gen9DeviceCaps, GlkIs32BitOsAllocatorAvailable) {
    const auto &caps = pDevice->getDeviceInfo();
    auto memoryManager = pDevice->getMemoryManager();
    if (is64bit) {
        EXPECT_TRUE(memoryManager->peekForce32BitAllocations());
        EXPECT_TRUE(caps.force32BitAddressess);
    } else {
        EXPECT_FALSE(memoryManager->peekForce32BitAllocations());
        EXPECT_FALSE(caps.force32BitAddressess);
    }
}

typedef Test<DeviceFixture> GlkUsDeviceIdTest;

GLKTEST_F(GlkUsDeviceIdTest, isSimulationCap) {
    unsigned short glkSimulationIds[3] = {
        IGLK_GT2_ULT_18EU_DEVICE_F0_ID,
        IGLK_GT2_ULT_12EU_DEVICE_F0_ID,
        0, // default, non-simulation
    };
    NEO::MockDevice *mockDevice = nullptr;
    for (auto id : glkSimulationIds) {
        mockDevice = createWithUsDeviceId(id);
        ASSERT_NE(mockDevice, nullptr);
        EXPECT_FALSE(mockDevice->isSimulation());
        delete mockDevice;
    }
}

GLKTEST_F(GlkUsDeviceIdTest, GivenGLKWhenCheckftr64KBpagesThenFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftr64KBpages);
}
