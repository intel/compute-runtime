/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/release_helper/release_helper_base.inl"

#include "platforms.h"
#include "release_definitions.h"

namespace NEO {
constexpr auto release = ReleaseType::release3001;
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
    if (debugManager.flags.Enable10ThreadsPerEu.get() == 0) {
        return 8u;
    }
    return 10;
}

template <>
bool ReleaseHelperHw<release>::isLocalOnlyAllowed() const {
    return false;
}

template <>
bool ReleaseHelperHw<release>::isDisablingMsaaRequired() const {
    return (hardwareIpVersion.value == AOT::PTL_U_A0);
}

template <>
bool ReleaseHelperHw<release>::getFtrXe2Compression() const {
    return !(hardwareIpVersion.value == AOT::PTL_U_A0);
}
} // namespace NEO

#include "shared/source/release_helper/release_helper_common_xe3_and_later.inl"
#include "shared/source/release_helper/release_helper_common_xe3_lpg.inl"
template class NEO::ReleaseHelperHw<NEO::release>;
