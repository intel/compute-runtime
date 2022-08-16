/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds_glk.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

typedef Test<ClDeviceFixture> Gen9DeviceCaps;

GLKTEST_F(Gen9DeviceCaps, WhenCheckingProfilingTimerResolutionThenCorrectResolutionIsReturned) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_EQ(52u, caps.outProfilingTimerResolution);
}

GLKTEST_F(Gen9DeviceCaps, givenGlkDeviceWhenAskedForDoubleSupportThenTrueIsReturned) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsFP64);
}

GLKTEST_F(Gen9DeviceCaps, GlkIs32BitOsAllocatorAvailable) {
    const auto &caps = pDevice->getDeviceInfo();
    auto memoryManager = pDevice->getMemoryManager();
    if constexpr (is64bit) {
        EXPECT_TRUE(memoryManager->peekForce32BitAllocations());
        EXPECT_TRUE(caps.force32BitAddressess);
    } else {
        EXPECT_FALSE(memoryManager->peekForce32BitAllocations());
        EXPECT_FALSE(caps.force32BitAddressess);
    }
}

typedef Test<ClDeviceFixture> GlkUsDeviceIdTest;

GLKTEST_F(GlkUsDeviceIdTest, WhenCheckingIsSimulationThenTrueReturnedOnlyForSimulationId) {
    unsigned short glkSimulationIds[3] = {
        0x3184,
        0x3185,
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

GLKTEST_F(GlkUsDeviceIdTest, givenGlkWhenCheckFtrSupportsInteger64BitAtomicsThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsInteger64BitAtomics);
}
