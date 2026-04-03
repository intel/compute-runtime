/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/string.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/release_helper/release_helper_base.inl"
#include "shared/source/xe3p_core/hw_cmds_base.h"

#include "release_definitions.h"

namespace NEO {
constexpr auto release = ReleaseType::release3511;
template <>
bool ReleaseHelperHw<release>::isBFloat16ConversionSupported() const {
    return true;
}

template <>
const SupportedNumGrfs ReleaseHelperHw<release>::getSupportedNumGrfs() const {
    if (debugManager.flags.Enable512NumGrfs.get()) {
        return {32u, 64u, 96u, 128u, 160u, 192u, 256u, 512u};
    }
    return {32u, 64u, 96u, 128u, 160u, 192u, 256u};
}

template <>
uint64_t ReleaseHelperHw<release>::getTotalMemBankSize() const {
    return 8ull * MemoryConstants::gigaByte;
}

template <>
const std::string ReleaseHelperHw<release>::getDeviceConfigString(uint32_t tileCount, uint32_t sliceCount, uint32_t subSliceCount, uint32_t euPerSubSliceCount) const {
    char configString[16] = {0};
    uint32_t xecuCount = 1;
    if (sliceCount > 4) {
        UNRECOVERABLE_IF(sliceCount % 4u != 0u);
        xecuCount = sliceCount / 4;
        sliceCount = sliceCount / xecuCount;
    }
    auto err = snprintf_s(configString, sizeof(configString), sizeof(configString), "%utx%ux%ux%ux%u", tileCount, xecuCount, sliceCount, subSliceCount, euPerSubSliceCount);
    UNRECOVERABLE_IF(err < 0);
    return configString;
}

template <>
bool ReleaseHelperHw<release>::isRayTracingSupported() const {
    return false;
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
bool ReleaseHelperHw<release>::getFtrXe2Compression() const {
    return false;
}

} // namespace NEO

#include "shared/source/release_helper/release_helper_common_xe3_and_later.inl"
#include "shared/source/release_helper/release_helper_common_xe3p.inl"
#include "shared/source/release_helper/release_helper_preferred_slm_xe3p_cri_384k.inl"
template class NEO::ReleaseHelperHw<NEO::release>;
