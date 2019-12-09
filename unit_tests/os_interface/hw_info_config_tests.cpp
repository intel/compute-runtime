/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/os_interface/hw_info_config_tests.h"

#include "core/helpers/hw_helper.h"
#include "core/helpers/options.h"

using namespace NEO;
using namespace std;

void HwInfoConfigTest::SetUp() {
    PlatformFixture::SetUp();

    pInHwInfo = pPlatform->getDevice(0)->getHardwareInfo();

    testPlatform = &pInHwInfo.platform;
    testSkuTable = &pInHwInfo.featureTable;
    testWaTable = &pInHwInfo.workaroundTable;
    testSysInfo = &pInHwInfo.gtSystemInfo;

    outHwInfo = {};
}

void HwInfoConfigTest::TearDown() {
    PlatformFixture::TearDown();
}

TEST_F(HwInfoConfigTest, givenHwInfoConfigSetHwInfoValuesFromConfigStringReturnsSetsProperValues) {
    bool success = setHwInfoValuesFromConfigString("2x4x16", outHwInfo);
    EXPECT_TRUE(success);
    EXPECT_EQ(outHwInfo.gtSystemInfo.SliceCount, 2u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.SubSliceCount, 8u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.EUCount, 128u);
}

TEST_F(HwInfoConfigTest, givenInvalidHwInfoSetHwInfoValuesFromConfigString) {
    bool success = setHwInfoValuesFromConfigString("1", outHwInfo);
    EXPECT_FALSE(success);

    success = setHwInfoValuesFromConfigString("1x3", outHwInfo);
    EXPECT_FALSE(success);

    success = setHwInfoValuesFromConfigString("65536x3x8", outHwInfo);
    EXPECT_FALSE(success);

    success = setHwInfoValuesFromConfigString("1x65536x8", outHwInfo);
    EXPECT_FALSE(success);

    success = setHwInfoValuesFromConfigString("1x3x65536", outHwInfo);
    EXPECT_FALSE(success);

    success = setHwInfoValuesFromConfigString("65535x65535x8", outHwInfo);
    EXPECT_FALSE(success);

    success = setHwInfoValuesFromConfigString("1x65535x65535", outHwInfo);
    EXPECT_FALSE(success);
}
