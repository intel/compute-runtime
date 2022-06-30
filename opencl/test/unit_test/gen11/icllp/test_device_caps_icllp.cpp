/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

using IcllpTest = Test<ClDeviceFixture>;

ICLLPTEST_F(IcllpTest, givenIcllpWhenSlmSizeIsRequiredThenReturnCorrectValue) {
    EXPECT_EQ(64u, pDevice->getHardwareInfo().capabilityTable.slmSize);
}

ICLLPTEST_F(IcllpTest, givenIclLpWhenCheckFtrSupportsInteger64BitAtomicsThenReturnFalse) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsInteger64BitAtomics);
}

ICLLPTEST_F(IcllpTest, WhenGettingPlatformFamilyThenIcelakeIsReported) {
    EXPECT_EQ(IGFX_ICELAKE_LP, pDevice->getHardwareInfo().platform.eProductFamily);
}

ICLLPTEST_F(IcllpTest, WhenCheckingExtensionStringThenFp64IsNotSupported) {
    const auto &caps = pClDevice->getDeviceInfo();
    std::string extensionString = caps.deviceExtensions;

    EXPECT_EQ(std::string::npos, extensionString.find(std::string("cl_khr_fp64")));
    EXPECT_EQ(0u, caps.doubleFpConfig);
}

ICLLPTEST_F(IcllpTest, WhenCheckingCapsThenCorrectlyRoundedDivideSqrtIsNotSupported) {
    const auto &caps = pClDevice->getDeviceInfo();
    EXPECT_EQ(0u, caps.singleFpConfig & CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT);
}

ICLLPTEST_F(IcllpTest, WhenCheckingSimulationCapThenResultIsCorrect) {
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
