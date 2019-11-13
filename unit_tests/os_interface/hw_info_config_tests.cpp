/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/os_interface/hw_info_config_tests.h"

#include "core/helpers/hw_helper.h"
#include "runtime/helpers/options.h"

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
