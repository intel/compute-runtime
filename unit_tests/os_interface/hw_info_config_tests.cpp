/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/os_interface/hw_info_config_tests.h"

#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/options.h"

using namespace NEO;
using namespace std;

void HwInfoConfigTest::SetUp() {
    PlatformFixture::SetUp();

    pInHwInfo = pPlatform->getDevice(0)->getHardwareInfo();

    testPlatform = &pInHwInfo.pPlatform;
    testSkuTable = &pInHwInfo.pSkuTable;
    testWaTable = &pInHwInfo.pWaTable;
    testSysInfo = &pInHwInfo.pSysInfo;

    outHwInfo = {};
}

void HwInfoConfigTest::TearDown() {
    PlatformFixture::TearDown();
}
