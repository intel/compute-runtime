/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_info.h"
#include "hw_cmds.h"

namespace OCLRT {
HardwareInfo::HardwareInfo(const PLATFORM *platform, const FeatureTable *skuTable, const WorkaroundTable *waTable,
                           const GT_SYSTEM_INFO *sysInfo, const RuntimeCapabilityTable &capabilityTable)
    : pPlatform(platform), pSkuTable(skuTable), pWaTable(waTable), pSysInfo(sysInfo), capabilityTable(capabilityTable) {
}

const HardwareInfo *hardwareInfoTable[IGFX_MAX_PRODUCT] = {};
void (*hardwareInfoSetup[IGFX_MAX_PRODUCT])(GT_SYSTEM_INFO *, FeatureTable *, bool) = {
    nullptr,
};

const FeatureTable emptySkuTable = {};
const WorkaroundTable emptyWaTable = {};

const char *getPlatformType(const HardwareInfo &hwInfo) {
    if (hwInfo.capabilityTable.isCore) {
        return "core";
    }
    return "lp";
}

bool getHwInfoForPlatformString(const char *str, const HardwareInfo *&hwInfoIn) {
    bool ret = false;
    for (int j = 0; j < IGFX_MAX_PRODUCT; j++) {
        if (hardwarePrefix[j] == nullptr)
            continue;
        if (strcmp(hardwarePrefix[j], str) == 0) {
            hwInfoIn = hardwareInfoTable[j];
            ret = true;
            break;
        }
    }
    return ret;
}
} // namespace OCLRT
