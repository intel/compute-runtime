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

typedef Test<DeviceFixture> SklDeviceCaps;

SKLTEST_F(SklDeviceCaps, reportsOcl21) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_STREQ("OpenCL 2.1 NEO ", caps.clVersion);
    EXPECT_STREQ("OpenCL C 2.0 ", caps.clCVersion);
}

SKLTEST_F(SklDeviceCaps, SklProfilingTimerResolution) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_EQ(83u, caps.profilingTimerResolution);
}

SKLTEST_F(SklDeviceCaps, givenSklDeviceWhenAskedFor32BitSupportThenFalseIsReturned) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_STREQ("OpenCL 2.1 NEO ", caps.clVersion);
    EXPECT_STREQ("OpenCL C 2.0 ", caps.clCVersion);

    auto memoryManager = pDevice->getMemoryManager();
    EXPECT_FALSE(memoryManager->peekForce32BitAllocations());
    EXPECT_FALSE(caps.force32BitAddressess);
}

SKLTEST_F(SklDeviceCaps, SklSvmCapabilities) {
    const auto &caps = pDevice->getDeviceInfo();
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
    OCLRT::MockDevice *mockDevice = nullptr;

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
