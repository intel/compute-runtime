/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds_lkf.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

using LkfTest = Test<ClDeviceFixture>;

LKFTEST_F(LkfTest, givenLkfWhenSlmSizeIsRequiredThenReturnCorrectValue) {
    EXPECT_EQ(64u, pDevice->getHardwareInfo().capabilityTable.slmSize);
}

LKFTEST_F(LkfTest, givenLkfWhenCheckedSvmSupportThenNoSvmIsReported) {
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

LKFTEST_F(LkfTest, WhenCheckingIsSimulationThenTrueReturnedOnlyForSimulationId) {
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
