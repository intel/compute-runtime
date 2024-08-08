/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"

#include <string>

namespace NEO {

template <PRODUCT_FAMILY product>
inline void AILConfigurationHw<product>::modifyKernelIfRequired(std::string &kernel) {
}

//  To avoid a known oneDNN issue in ZEBin handling,
//  fall back to legacy (patchtoken) format when dummy kernel used by nGen is detected.
//  Only this specific kernel with that exact source code will be affected.

template <PRODUCT_FAMILY product>
inline bool AILConfigurationHw<product>::isFallbackToPatchtokensRequired(const std::string &kernelSources) {
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

template <PRODUCT_FAMILY product>
inline void AILConfigurationHw<product>::applyExt(RuntimeCapabilityTable &runtimeCapabilityTable) {
}

template <PRODUCT_FAMILY product>
inline bool AILConfigurationHw<product>::isContextSyncFlagRequired() {
    return false;
}

template <PRODUCT_FAMILY product>
inline bool AILConfigurationHw<product>::isBufferPoolEnabled() {
    return true;
}

template <PRODUCT_FAMILY product>
inline bool AILConfigurationHw<product>::useLegacyValidationLogic() {
    return false;
}

template <PRODUCT_FAMILY product>
inline bool AILConfigurationHw<product>::forceRcs() {
    return shouldForceRcs;
}

template <PRODUCT_FAMILY product>
inline bool AILConfigurationHw<product>::handleDivergentBarriers() {
    return shouldHandleDivergentBarriers;
}
template <PRODUCT_FAMILY product>
inline void AILConfigurationHw<product>::setHandleDivergentBarriers(bool val) {
    shouldHandleDivergentBarriers = val;
}

} // namespace NEO
