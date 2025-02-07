/*
 * Copyright (C) 2022-2025 Intel Corporation
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

template <PRODUCT_FAMILY product>
inline void AILConfigurationHw<product>::applyExt(HardwareInfo &hwInfo) {
}

template <PRODUCT_FAMILY product>
inline bool AILConfigurationHw<product>::isContextSyncFlagRequired() {
    return false;
}

template <PRODUCT_FAMILY product>
inline bool AILConfigurationHw<product>::isRunAloneContextRequired() {
    return false;
}

template <PRODUCT_FAMILY product>
inline bool AILConfigurationHw<product>::is256BPrefetchDisableRequired() {
    return false;
}

template <PRODUCT_FAMILY product>
inline bool AILConfigurationHw<product>::drainHostptrs() {
    return true;
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
inline bool AILConfigurationHw<product>::disableBindlessAddressing() {
    return shouldDisableBindlessAddressing;
}
template <PRODUCT_FAMILY product>
inline void AILConfigurationHw<product>::setHandleDivergentBarriers(bool val) {
    shouldHandleDivergentBarriers = val;
}
template <PRODUCT_FAMILY product>
inline void AILConfigurationHw<product>::setDisableBindlessAddressing(bool val) {
    shouldDisableBindlessAddressing = val;
}

template <PRODUCT_FAMILY product>
inline bool AILConfigurationHw<product>::limitAmountOfDeviceMemoryForRecycling() {
    return false;
}

template <PRODUCT_FAMILY product>
inline bool AILConfigurationHw<product>::isAdjustMicrosecondResolutionRequired() {
    return shouldAdjustMicrosecondResolution;
}

template <PRODUCT_FAMILY product>
inline uint32_t AILConfigurationHw<product>::getMicrosecondResolution() {
    return microsecondAdjustment;
}

} // namespace NEO
