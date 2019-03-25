/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"

using namespace OCLRT;

struct HwInfoHelper {
    HwInfoHelper() {
        memcpy(&testPlatform, platformDevices[0]->pPlatform, sizeof(testPlatform));
        memcpy(&testFtrTable, platformDevices[0]->pSkuTable, sizeof(testFtrTable));
        memcpy(&testWaTable, platformDevices[0]->pWaTable, sizeof(testWaTable));
        memcpy(&testSysInfo, platformDevices[0]->pSysInfo, sizeof(testSysInfo));
        hwInfo.capabilityTable = platformDevices[0]->capabilityTable;
        hwInfo.pPlatform = &testPlatform;
        hwInfo.pSkuTable = &testFtrTable;
        hwInfo.pSysInfo = &testSysInfo;
        hwInfo.pWaTable = &testWaTable;
    }

    PLATFORM testPlatform;
    FeatureTable testFtrTable;
    WorkaroundTable testWaTable;
    GT_SYSTEM_INFO testSysInfo;
    HardwareInfo hwInfo;
};
