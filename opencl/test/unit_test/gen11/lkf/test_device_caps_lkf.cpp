/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "test.h"

using namespace NEO;

using LkfTest = Test<DeviceFixture>;

LKFTEST_F(LkfTest, givenLkfWhenSlmSizeIsRequiredThenReturnCorrectValue) {
    EXPECT_EQ(64u, pDevice->getHardwareInfo().capabilityTable.slmSize);
}

LKFTEST_F(LkfTest, givenLKFWhenCheckedOCLVersionThen21IsReported) {
    const auto &caps = pClDevice->getDeviceInfo();
    EXPECT_STREQ("OpenCL 1.2 NEO ", caps.clVersion);
    EXPECT_STREQ("OpenCL C 1.2 ", caps.clCVersion);
}

LKFTEST_F(LkfTest, givenLKFWhenCheckedSvmSupportThenNoSvmIsReported) {
    const auto &caps = pClDevice->getDeviceInfo();
    EXPECT_EQ(caps.svmCapabilities, 0u);
}

LKFTEST_F(LkfTest, givenLkfWhenDoublePrecissionIsCheckedThenFalseIsReturned) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsFP64);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupports64BitMath);
}

LKFTEST_F(LkfTest, givenLkfWhenExtensionStringIsCheckedThenFP64IsNotReported) {
    const auto &caps = pClDevice->getDeviceInfo();
    std::string extensionString = caps.deviceExtensions;

    EXPECT_EQ(std::string::npos, extensionString.find(std::string("cl_khr_fp64")));
    EXPECT_EQ(0u, caps.doubleFpConfig);
}

LKFTEST_F(LkfTest, isSimulationCap) {
    unsigned short lkfSimulationIds[2] = {
        ILKF_1x8x8_DESK_DEVICE_F0_ID,
        0, // default, non-simulation
    };
    NEO::MockDevice *mockDevice = nullptr;

    for (auto id : lkfSimulationIds) {
        mockDevice = createWithUsDeviceId(id);
        ASSERT_NE(mockDevice, nullptr);

        if (id == 0)
            EXPECT_FALSE(mockDevice->isSimulation());
        else
            EXPECT_TRUE(mockDevice->isSimulation());
        delete mockDevice;
    }
}
