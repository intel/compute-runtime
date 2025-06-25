/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/release_helper/release_helper_base.inl"

#include "neo_aot_platforms.h"
#include "release_definitions.h"

namespace NEO {
constexpr auto release = ReleaseType::release3000;
template <>
bool ReleaseHelperHw<release>::isBFloat16ConversionSupported() const {
    return true;
}

template <>
std::vector<uint32_t> ReleaseHelperHw<release>::getSupportedNumGrfs() const {
    return {32u, 64u, 96u, 128u, 160u, 192u, 256u};
}

template <>
uint32_t ReleaseHelperHw<release>::getNumThreadsPerEu() const {
    if (debugManager.flags.OverrideNumThreadsPerEu.get() != -1) {
        return debugManager.flags.OverrideNumThreadsPerEu.get();
    }
    return 10;
}

template <>
bool ReleaseHelperHw<release>::isLocalOnlyAllowed() const {
    return false;
}

template <>
uint32_t ReleaseHelperHw<release>::getAsyncStackSizePerRay() const {
    return 64u;
}

template <>
bool ReleaseHelperHw<release>::getFtrXe2Compression() const {
    return !(hardwareIpVersion.value == AOT::PTL_H_A0);
}

template <>
bool ReleaseHelperHw<release>::isBindlessAddressingDisabled() const {
    return false;
}

template <>
bool ReleaseHelperHw<release>::isGlobalBindlessAllocatorEnabled() const {
    return true;
}

} // namespace NEO

#include "shared/source/release_helper/release_helper_common_xe3_and_later.inl"
#include "shared/source/release_helper/release_helper_common_xe3_lpg.inl"
template class NEO::ReleaseHelperHw<NEO::release>;
