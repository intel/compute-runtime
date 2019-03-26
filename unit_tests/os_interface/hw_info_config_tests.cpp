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

    const HardwareInfo &hwInfo = pPlatform->getDevice(0)->getHardwareInfo();
    pInHwInfo = const_cast<HardwareInfo *>(&hwInfo);

    originalCapTable = pInHwInfo->capabilityTable;

    pOldPlatform = pInHwInfo->pPlatform;
    memcpy(&testPlatform, pOldPlatform, sizeof(testPlatform));
    pInHwInfo->pPlatform = &testPlatform;

    pOldSkuTable = pInHwInfo->pSkuTable;
    memcpy(&testSkuTable, pOldSkuTable, sizeof(testSkuTable));
    pInHwInfo->pSkuTable = &testSkuTable;

    pOldWaTable = pInHwInfo->pWaTable;
    memcpy(&testWaTable, pOldWaTable, sizeof(testWaTable));
    pInHwInfo->pWaTable = &testWaTable;

    pOldSysInfo = pInHwInfo->pSysInfo;
    memcpy(&testSysInfo, pOldSysInfo, sizeof(testSysInfo));
    pInHwInfo->pSysInfo = &testSysInfo;

    outHwInfo = {};
}

void HwInfoConfigTest::TearDown() {
    ReleaseOutHwInfoStructs();

    pInHwInfo->pPlatform = pOldPlatform;
    pInHwInfo->pSkuTable = pOldSkuTable;
    pInHwInfo->pWaTable = pOldWaTable;
    pInHwInfo->pSysInfo = pOldSysInfo;

    pInHwInfo->capabilityTable = originalCapTable;

    PlatformFixture::TearDown();
}

void HwInfoConfigTest::ReleaseOutHwInfoStructs() {
    if (outHwInfo.pPlatform != nullptr) {
        delete outHwInfo.pPlatform;
        outHwInfo.pPlatform = nullptr;
    }
    if (outHwInfo.pSkuTable != nullptr) {
        delete outHwInfo.pSkuTable;
        outHwInfo.pSkuTable = nullptr;
    }
    if (outHwInfo.pWaTable != nullptr) {
        delete outHwInfo.pWaTable;
        outHwInfo.pWaTable = nullptr;
    }
    if (outHwInfo.pSysInfo != nullptr) {
        delete outHwInfo.pSysInfo;
        outHwInfo.pSysInfo = nullptr;
    }
}
