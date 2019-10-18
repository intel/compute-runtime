/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hw_info.h"

#include "runtime/gen_common/hw_cmds.h"
#include "runtime/os_interface/debug_settings_manager.h"

#include <algorithm>

namespace NEO {
HardwareInfo::HardwareInfo(const PLATFORM *platform, const FeatureTable *featureTable, const WorkaroundTable *workaroundTable,
                           const GT_SYSTEM_INFO *gtSystemInfo, const RuntimeCapabilityTable &capabilityTable)
    : platform(*platform), featureTable(*featureTable), workaroundTable(*workaroundTable), gtSystemInfo(*gtSystemInfo), capabilityTable(capabilityTable) {
}

// Global table of hardware prefixes
const char *hardwarePrefix[IGFX_MAX_PRODUCT] = {
    nullptr,
};

// Global table of default hardware info configs
const std::string *defaultHardwareInfoConfigTable[IGFX_MAX_PRODUCT] = {
    nullptr,
};

// Global table of family names
const char *familyName[IGFX_MAX_CORE] = {
    nullptr,
};
// Global table of family names
bool familyEnabled[IGFX_MAX_CORE] = {
    false,
};

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

bool setHwInfoValuesFromConfigString(const std::string &hwInfoConfig, HardwareInfo &hwInfoIn) {
    size_t currPos = hwInfoConfig.find('x', 0);
    if (currPos == std::string::npos) {
        return false;
    }
    uint32_t sliceCount = static_cast<uint32_t>(std::stoul(hwInfoConfig.substr(0, currPos)));
    size_t prevPos = currPos + 1;

    currPos = hwInfoConfig.find('x', prevPos);
    if (currPos == std::string::npos) {
        return false;
    }
    uint32_t subSliceCount = sliceCount * static_cast<uint32_t>(std::stoul(hwInfoConfig.substr(prevPos, currPos)));
    prevPos = currPos + 1;

    uint32_t euCount = subSliceCount * static_cast<uint32_t>(std::stoul(hwInfoConfig.substr(prevPos, std::string::npos)));

    hwInfoIn.gtSystemInfo.SliceCount = sliceCount;
    hwInfoIn.gtSystemInfo.SubSliceCount = subSliceCount;
    hwInfoIn.gtSystemInfo.EUCount = euCount;

    return true;
}

aub_stream::EngineType getChosenEngineType(const HardwareInfo &hwInfo) {
    return DebugManager.flags.NodeOrdinal.get() == -1
               ? hwInfo.capabilityTable.defaultEngineType
               : static_cast<aub_stream::EngineType>(DebugManager.flags.NodeOrdinal.get());
}

const std::string getFamilyNameWithType(const HardwareInfo &hwInfo) {
    std::string platformName = familyName[hwInfo.platform.eRenderCoreFamily];
    platformName.append(hwInfo.capabilityTable.platformType);
    return platformName;
}
} // namespace NEO
