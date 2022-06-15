/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/source/helpers/hash.h"

#include <map>
#include <string_view>

namespace NEO {
/*
 * fp64 support is unavailable on some Intel GPUs, and the SW emulation in IGC should not be enabled by default.
 * For Blender, fp64 is not performance-critical - SW emulation is good enough for the application to be usable
 * (some versions would not function correctly without it).
 *
 */

std::map<std::string_view, std::vector<AILEnumeration>> applicationMap = {{"blender", {AILEnumeration::ENABLE_FP64}}};

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
            default:
                break;
            }
        }
    }

    applyExt(runtimeCapabilityTable);
}

} // namespace NEO
