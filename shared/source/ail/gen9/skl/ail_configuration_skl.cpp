/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration_base.inl"
#include "shared/source/helpers/hw_info.h"

#include <map>
#include <vector>

namespace NEO {
static EnableAIL<IGFX_SKYLAKE> enableAILSKL;

std::map<std::string_view, std::vector<AILEnumeration>> applicationMapSKL = {
    {"resolve", {AILEnumeration::disableHostPtrTracking}} // Disable hostPtrTracking for DaVinci Resolve
};

template <>
inline void AILConfigurationHw<IGFX_SKYLAKE>::applyExt(RuntimeCapabilityTable &runtimeCapabilityTable) {
    auto search = applicationMapSKL.find(processName);
    if (search != applicationMapSKL.end()) {
        for (size_t i = 0; i < search->second.size(); ++i) {
            switch (search->second[i]) {
            case AILEnumeration::disableHostPtrTracking:
                runtimeCapabilityTable.hostPtrTrackingEnabled = false;
                break;
            default:
                break;
            }
        }
    }
}

template class AILConfigurationHw<IGFX_SKYLAKE>;
} // namespace NEO
