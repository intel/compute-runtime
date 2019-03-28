/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_info.h"

#include "runtime/os_interface/debug_settings_manager.h"

#include "hw_cmds.h"

namespace NEO {
HardwareInfo::HardwareInfo(const PLATFORM *platform, const FeatureTable *skuTable, const WorkaroundTable *waTable,
                           const GT_SYSTEM_INFO *sysInfo, const RuntimeCapabilityTable &capabilityTable)
    : pPlatform(platform), pSkuTable(skuTable), pWaTable(waTable), pSysInfo(sysInfo), capabilityTable(capabilityTable) {
}

const HardwareInfo *hardwareInfoTable[IGFX_MAX_PRODUCT] = {};
void (*hardwareInfoSetup[IGFX_MAX_PRODUCT])(HardwareInfo *, bool, const std::string &) = {
    nullptr,
};

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

aub_stream::EngineType getChosenEngineType(const HardwareInfo &hwInfo) {
    return DebugManager.flags.NodeOrdinal.get() == -1
               ? hwInfo.capabilityTable.defaultEngineType
               : static_cast<aub_stream::EngineType>(DebugManager.flags.NodeOrdinal.get());
}
} // namespace NEO
