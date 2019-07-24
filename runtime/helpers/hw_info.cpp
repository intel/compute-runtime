/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_info.h"

#include "runtime/os_interface/debug_settings_manager.h"

#include "hw_cmds.h"

#include <algorithm>

namespace NEO {
HardwareInfo::HardwareInfo(const PLATFORM *platform, const FeatureTable *featureTable, const WorkaroundTable *workaroundTable,
                           const GT_SYSTEM_INFO *gtSystemInfo, const RuntimeCapabilityTable &capabilityTable)
    : platform(*platform), featureTable(*featureTable), workaroundTable(*workaroundTable), gtSystemInfo(*gtSystemInfo), capabilityTable(capabilityTable) {
}

const HardwareInfo *hardwareInfoTable[IGFX_MAX_PRODUCT] = {};
void (*hardwareInfoSetup[IGFX_MAX_PRODUCT])(HardwareInfo *, bool, const std::string &) = {
    nullptr,
};

bool getHwInfoForPlatformString(std::string &platform, const HardwareInfo *&hwInfoIn) {
    std::transform(platform.begin(), platform.end(), platform.begin(), ::tolower);

    bool ret = false;
    for (int j = 0; j < IGFX_MAX_PRODUCT; j++) {
        if (hardwarePrefix[j] == nullptr)
            continue;
        if (hardwarePrefix[j] == platform) {
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
