/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/source/helpers/hash.h"
#include "shared/source/helpers/hw_info.h"

#include <map>
#include <vector>

namespace NEO {
/*
 * fp64 support is unavailable on some Intel GPUs, and the SW emulation in IGC should not be enabled by default.
 * For Blender, fp64 is not performance-critical - SW emulation is good enough for the application to be usable
 * (some versions would not function correctly without it).
 *
 */

std::map<std::string_view, std::vector<AILEnumeration>> applicationMap = {{"blender", {AILEnumeration::ENABLE_FP64}},
                                                                          // Modify reported platform name to ensure older versions of Adobe Premiere Pro are able to recognize the GPU device
                                                                          {"Adobe Premiere Pro", {AILEnumeration::ENABLE_LEGACY_PLATFORM_NAME}}};

std::map<std::string_view, std::vector<AILEnumeration>> applicationMapMTL = {{"svchost", {AILEnumeration::DISABLE_DIRECT_SUBMISSION}}};

const std::set<std::string_view> applicationsContextSyncFlag = {};

AILConfiguration *ailConfigurationTable[IGFX_MAX_PRODUCT] = {};

AILConfiguration *AILConfiguration::get(PRODUCT_FAMILY productFamily) {
    return ailConfigurationTable[productFamily];
}

void AILConfiguration::apply(RuntimeCapabilityTable &runtimeCapabilityTable) {
    auto search = applicationMap.find(processName);

    if (search != applicationMap.end()) {
        for (size_t i = 0; i < search->second.size(); ++i) {
            switch (search->second[i]) {
            case AILEnumeration::ENABLE_FP64:
                runtimeCapabilityTable.ftrSupportsFP64 = true;
                break;
            case AILEnumeration::ENABLE_LEGACY_PLATFORM_NAME:
                runtimeCapabilityTable.preferredPlatformName = legacyPlatformName;
                break;
            default:
                break;
            }
        }
    }

    applyExt(runtimeCapabilityTable);
}

} // namespace NEO
