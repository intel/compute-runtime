/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/release_helper/release_helper_base.inl"
#include "shared/source/xe3p_core/hw_cmds_base.h"

#include "neo_aot_platforms.h"
#include "release_definitions.h"

namespace NEO {
template <>
bool ReleaseHelperHw<release>::isBFloat16ConversionSupported() const {
    return true;
}

template <>
const SupportedNumGrfs ReleaseHelperHw<release>::getSupportedNumGrfs() const {
    if (!(hardwareIpVersion.value == AOT::NVL_P_A0)) {
        return {32u, 64u, 96u, 128u, 160u, 192u, 256u, 320u, 448u, 512u};
    }

    return {32u, 64u, 96u, 128u, 160u, 192u, 256u, 512u};
}

template <>
uint32_t ReleaseHelperHw<release>::getAdditionalFp16Caps() const {
    return FpAtomicExtFlags::addAtomicCaps;
}

template <>
uint32_t ReleaseHelperHw<release>::getAdditionalExtraCaps() const {
    return (FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps | FpAtomicExtFlags::addAtomicCaps);
}

template <>
bool ReleaseHelperHw<release>::isLocalOnlyAllowed() const {
    return false;
}

template <>
uint32_t ReleaseHelperHw<release>::getStackSizePerRay() const {
    return 64u;
}

template <>
const std::string ReleaseHelperHw<release>::getDeviceConfigString(uint32_t tileCount, uint32_t sliceCount, uint32_t subSliceCount, uint32_t euPerSubSliceCount) const {
    char configString[16] = {0};
    auto err = snprintf_s(configString, sizeof(configString), sizeof(configString), "%utx%ux%ux%u", tileCount, sliceCount, subSliceCount, euPerSubSliceCount);
    UNRECOVERABLE_IF(err < 0);
    return configString;
}

template <>
bool ReleaseHelperHw<release>::isNumRtStacksPerDssFixedValue() const {
    return false;
}

template <>
bool ReleaseHelperHw<release>::isGlobalBindlessAllocatorEnabled() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::isPostImageWriteFlushRequired() const {
    return true;
}

template <>
uint32_t ReleaseHelperHw<release>::adjustMaxThreadsPerEuCount(uint32_t maxThreadsPerEuCount, uint32_t grfCount) const {
    auto adjustedMaxThreadsPerEuCount = maxThreadsPerEuCount;

    if (!(hardwareIpVersion.value == AOT::NVL_P_A0)) {
        if (grfCount <= 256) {
            // do nothing
        } else if (grfCount <= 320u) {
            adjustedMaxThreadsPerEuCount = 3;
        } else if (grfCount <= 448u) {
            adjustedMaxThreadsPerEuCount = 2;
        }
    }

    return adjustedMaxThreadsPerEuCount;
}

template <>
bool ReleaseHelperHw<release>::isStateCacheInvalidationWaRequired(bool isImmediateCmdList, bool kernelUsesImageOrSampler) const {
    auto enableStateCacheInvalidationWa = debugManager.flags.EnableStateCacheInvalidationWa.get();
    if (enableStateCacheInvalidationWa != -1) {
        return enableStateCacheInvalidationWa;
    }
    return (hardwareIpVersion.value == AOT::NVL_P_A0);
}

template <>
bool ReleaseHelperHw<release>::isAvailableSemaphore64Base() const {
    return static_cast<bool>(hardwareIpVersion.value != AOT::NVL_P_A0);
}

template <>
bool ReleaseHelperHw<release>::isPreImageReadFlushRequired() const {
    return true;
}

} // namespace NEO
