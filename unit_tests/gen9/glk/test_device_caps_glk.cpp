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

#include "unit_tests/fixtures/device_fixture.h"
#include "test.h"

using namespace OCLRT;

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
    if (is32BitOsAllocatorAvailable) {
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
    OCLRT::MockDevice *mockDevice = nullptr;
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
