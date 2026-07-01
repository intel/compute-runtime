/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

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
    return {32u, 64u, 96u, 128u, 160u, 192u, 256u, 512u};
}

template <>
uint64_t ReleaseHelperHw<release>::getTotalMemBankSize() const {
    return 8ull * MemoryConstants::gigaByte;
}

template <>
bool ReleaseHelperHw<release>::isDeviceConfigStringTileCountIncluded() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::isDeviceConfigStringXeCuSegmentIncluded() const {
    return true;
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
