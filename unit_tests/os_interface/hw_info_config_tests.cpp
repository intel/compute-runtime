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

#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/options.h"

#include "unit_tests/os_interface/hw_info_config_tests.h"

using namespace OCLRT;
using namespace std;

void HwInfoConfigTest::SetUp() {
    PlatformFixture::SetUp(numPlatformDevices, platformDevices);

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
