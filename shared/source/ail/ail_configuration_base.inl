/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"

#include <string>

namespace NEO {

template <PRODUCT_FAMILY Product>
inline void AILConfigurationHw<Product>::modifyKernelIfRequired(std::string &kernel) {
}

//  To avoid a known oneDNN issue in ZEBin handling,
//  fall back to legacy (patchtoken) format when dummy kernel used by nGen is detected.
//  Only this specific kernel with that exact source code will be affected.

template <PRODUCT_FAMILY Product>
inline bool AILConfigurationHw<Product>::isFallbackToPatchtokensRequired(const std::string &kernelSources) {
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

template <PRODUCT_FAMILY Product>
inline void AILConfigurationHw<Product>::applyExt(RuntimeCapabilityTable &runtimeCapabilityTable) {
}

template <PRODUCT_FAMILY Product>
inline bool AILConfigurationHw<Product>::isContextSyncFlagRequired() {
    return false;
}

template <PRODUCT_FAMILY Product>
inline bool AILConfigurationHw<Product>::useLegacyValidationLogic() {
    return false;
}

} // namespace NEO
