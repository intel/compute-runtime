/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration_base.inl"

#include <map>
#include <vector>

namespace NEO {
static EnableAIL<IGFX_TIGERLAKE_LP> enableAILTGLLP;

std::map<std::string_view, std::vector<AILEnumeration>> applicationMapTGLLP = {};

//  To avoid a known oneDNN issue in ZEBin handling, affecting ICL and TGL platforms,
//  fall back to legacy (patchtoken) format when dummy kernel used by nGen is detected.
//  Only this specific kernel with that exact source code will be affected.

template <>
inline void AILConfigurationHw<IGFX_TIGERLAKE_LP>::forceFallbackToPatchtokensIfRequired(const std::string &kernelSources, bool &requiresFallback) {
    std::string_view dummyKernelSource{"kernel void _(){}"};
    if (sourcesContain(kernelSources, dummyKernelSource)) {
        requiresFallback = true;
    }
}

template class AILConfigurationHw<IGFX_TIGERLAKE_LP>;

} // namespace NEO
