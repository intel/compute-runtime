/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration_base.inl"
#include "shared/source/helpers/hw_info.h"

#include <map>
#include <vector>

namespace NEO {
static EnableAIL<IGFX_ICELAKE_LP> enableAILICLLP;

std::map<std::string_view, std::vector<AILEnumeration>> applicationMapICLLP = {
    {"resolve", {AILEnumeration::DISABLE_HOST_PTR_TRACKING}} // Disable hostPtrTracking for DaVinci Resolve
};

template <>
inline void AILConfigurationHw<IGFX_ICELAKE_LP>::applyExt(RuntimeCapabilityTable &runtimeCapabilityTable) {
    auto search = applicationMapICLLP.find(processName);
    if (search != applicationMapICLLP.end()) {
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

//  To avoid a known oneDNN issue in ZEBin handling, affecting ICL and TGL platforms,
//  fall back to legacy (patchtoken) format when dummy kernel used by nGen is detected.
//  Only this specific kernel with that exact source code will be affected.

template <>
inline void AILConfigurationHw<IGFX_ICELAKE_LP>::forceFallbackToPatchtokensIfRequired(const std::string &kernelSources, bool &setFallback) {
    std::string_view dummyKernelSource{"kernel void _(){}"};
    if (sourcesContain(kernelSources, dummyKernelSource)) {
        setFallback = true;
    }
}

template class AILConfigurationHw<IGFX_ICELAKE_LP>;
} // namespace NEO
