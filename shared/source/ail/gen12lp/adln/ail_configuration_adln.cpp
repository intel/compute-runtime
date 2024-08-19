/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration_base.inl"

#include <map>
#include <vector>

namespace NEO {
static EnableAIL<IGFX_ALDERLAKE_N> enableAILADLN;

std::map<std::string_view, std::vector<AILEnumeration>> applicationMapADLN = {};

template <>
inline bool AILConfigurationHw<IGFX_ALDERLAKE_N>::isFallbackToPatchtokensRequired(const std::string &kernelSources) {
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

template class AILConfigurationHw<IGFX_ALDERLAKE_N>;

} // namespace NEO
