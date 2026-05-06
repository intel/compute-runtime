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
const std::string ReleaseHelperHw<release>::getDeviceConfigString(uint32_t tileCount, uint32_t sliceCount, uint32_t subSliceCount, uint32_t euPerSubSliceCount) const {
    char configString[16] = {0};
    auto err = snprintf_s(configString, sizeof(configString), sizeof(configString), "%utx%ux%ux%u", tileCount, sliceCount, subSliceCount, euPerSubSliceCount);
    UNRECOVERABLE_IF(err < 0);
    return configString;
}

template <>
bool ReleaseHelperHw<release>::isGlobalBindlessAllocatorEnabled() const {
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
