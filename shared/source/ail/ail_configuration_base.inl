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

} // namespace NEO
