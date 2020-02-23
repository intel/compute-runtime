/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "test.h"

using namespace NEO;

using IcllpTest = Test<DeviceFixture>;

ICLLPTEST_F(IcllpTest, givenIcllpWhenSlmSizeIsRequiredThenReturnCorrectValue) {
    EXPECT_EQ(64u, pDevice->getHardwareInfo().capabilityTable.slmSize);
}

ICLLPTEST_F(IcllpTest, givenIclLpWhenCheckFtrSupportsInteger64BitAtomicsThenReturnFalse) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsInteger64BitAtomics);
}

ICLLPTEST_F(IcllpTest, shouldPassOnIcllp) {
    EXPECT_EQ(IGFX_ICELAKE_LP, pDevice->getHardwareInfo().platform.eProductFamily);
}

ICLLPTEST_F(IcllpTest, lpSkusDontSupportFP64) {
    const auto &caps = pDevice->getDeviceInfo();
    std::string extensionString = caps.deviceExtensions;

    EXPECT_EQ(std::string::npos, extensionString.find(std::string("cl_khr_fp64")));
    EXPECT_EQ(0u, caps.doubleFpConfig);
}

ICLLPTEST_F(IcllpTest, lpSkusDontSupportCorrectlyRoundedDivideSqrt) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_EQ(0u, caps.singleFpConfig & CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT);
}

ICLLPTEST_F(IcllpTest, isSimulationCap) {
    unsigned short iclLpSimulationIds[2] = {
        IICL_LP_GT1_MOB_DEVICE_F0_ID,
        0, // default, non-simulation
    };
    NEO::MockDevice *mockDevice = nullptr;

    for (auto id : iclLpSimulationIds) {
        mockDevice = createWithUsDeviceId(id);
        ASSERT_NE(mockDevice, nullptr);

        if (id == 0)
            EXPECT_FALSE(mockDevice->isSimulation());
        else
            EXPECT_TRUE(mockDevice->isSimulation());
        delete mockDevice;
    }
}

ICLLPTEST_F(IcllpTest, GivenICLLPWhenCheckftr64KBpagesThenFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftr64KBpages);
}
