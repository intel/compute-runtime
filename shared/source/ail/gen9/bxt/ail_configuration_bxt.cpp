/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration_base.inl"
#include "shared/source/helpers/hw_info.h"

#include <map>
#include <vector>

namespace NEO {
static EnableAIL<IGFX_BROXTON> enableAILBXT;

std::map<std::string_view, std::vector<AILEnumeration>> applicationMapBXT = {
    {"resolve", {AILEnumeration::DISABLE_HOST_PTR_TRACKING}} // Disable hostPtrTracking for DaVinci Resolve
};

template <>
inline void AILConfigurationHw<IGFX_BROXTON>::applyExt(RuntimeCapabilityTable &runtimeCapabilityTable) {
    auto search = applicationMapBXT.find(processName);
    if (search != applicationMapBXT.end()) {
        for (size_t i = 0; i < search->second.size(); ++i) {
            switch (search->second[i]) {
            case AILEnumeration::DISABLE_HOST_PTR_TRACKING:
                runtimeCapabilityTable.hostPtrTrackingEnabled = false;
                break;
            default:
                break;
            }
        }
    }
}

template class AILConfigurationHw<IGFX_BROXTON>;
} // namespace NEO
