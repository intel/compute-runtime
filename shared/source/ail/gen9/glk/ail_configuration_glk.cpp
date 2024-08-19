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
static EnableAIL<IGFX_GEMINILAKE> enableAILGLK;

std::map<std::string_view, std::vector<AILEnumeration>> applicationMapGLK = {
    {"resolve", {AILEnumeration::disableHostPtrTracking}} // Disable hostPtrTracking for DaVinci Resolve
};

template <>
inline void AILConfigurationHw<IGFX_GEMINILAKE>::applyExt(RuntimeCapabilityTable &runtimeCapabilityTable) {
    auto search = applicationMapGLK.find(processName);
    if (search != applicationMapGLK.end()) {
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

template <>
inline bool AILConfigurationHw<IGFX_GEMINILAKE>::isFallbackToPatchtokensRequired(const std::string &kernelSources) {
    std::string_view dummyKernelSource{"kernel void _(){}"};
    if (sourcesContain(kernelSources, dummyKernelSource)) {
        return true;
    }

    for (const auto &name : {"Resolve",
                             "ArcControlAssist",
                             "ArcControl"}) {
        if (processName == name) {
            return true;
        }
    }
    return false;
}

template class AILConfigurationHw<IGFX_GEMINILAKE>;
} // namespace NEO
