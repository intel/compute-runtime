/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/string.h"
#include "shared/source/release_helper/release_helper.h"

namespace NEO {

template <>
bool ReleaseHelperHw<release>::isBFloat16ConversionSupported() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::isRcsExposureDisabled() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::isDeviceConfigStringTileCountIncluded() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::isGlobalBindlessAllocatorEnabled() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::isPreImageReadFlushRequired() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::isStateCacheInvalidationWaRequired(bool isImmediateCmdList, bool kernelUsesImageOrSampler) const {
    auto enableStateCacheInvalidationWa = debugManager.flags.EnableStateCacheInvalidationWa.get();
    if (enableStateCacheInvalidationWa != -1) {
        return enableStateCacheInvalidationWa;
    }
    return true;
}

} // namespace NEO
