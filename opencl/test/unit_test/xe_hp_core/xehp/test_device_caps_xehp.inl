/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

#include "gtest/gtest.h"

using namespace NEO;

typedef Test<ClDeviceFixture> XeHPUsDeviceIdTest;

XEHPTEST_F(XeHPUsDeviceIdTest, WhenGettingHardwareInfoThenProductFamilyIsXeHpSdv) {
    EXPECT_EQ(IGFX_XE_HP_SDV, pDevice->getHardwareInfo().platform.eProductFamily);
}

XEHPTEST_F(XeHPUsDeviceIdTest, givenXeHPWhenCheckftr64KBpagesThenTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftr64KBpages);
}

XEHPTEST_F(XeHPUsDeviceIdTest, WheCheckingIsSimulationThenFalseIsReturned) {
    EXPECT_FALSE(pDevice->isSimulation());
}

XEHPTEST_F(XeHPUsDeviceIdTest, givenXeHPSkusThenItSupportCorrectlyRoundedDivSqrtBit) {
    EXPECT_TRUE(pClDevice->getHardwareInfo().capabilityTable.ftrSupports64BitMath);
    cl_device_fp_config actual = pClDevice->getDeviceInfo().singleFpConfig & CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT;
    EXPECT_NE(0ull, actual);
}

XEHPTEST_F(XeHPUsDeviceIdTest, givenXeHPWhenCheckSupportCacheFlushAfterWalkerThenTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.supportCacheFlushAfterWalker);
}
