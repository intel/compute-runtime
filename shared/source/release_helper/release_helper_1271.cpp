/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"
#include "shared/source/release_helper/release_helper_base.inl"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

#include "neo_aot_platforms.h"
#include "release_definitions.h"

#include <algorithm>

namespace NEO {
constexpr auto release = ReleaseType::release1271;

template <>
bool ReleaseHelperHw<release>::isPipeControlPriorToNonPipelinedStateCommandsWARequired() const {
    return hardwareIpVersion.value == AOT::MTL_H_A0;
}

template <>
bool ReleaseHelperHw<release>::isProgramAllStateComputeCommandFieldsWARequired() const {
    return hardwareIpVersion.value == AOT::MTL_H_A0;
}

template <>
inline bool ReleaseHelperHw<release>::isAuxSurfaceModeOverrideRequired() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::isDirectSubmissionLightSupported() const {
    return true;
}

template <>
inline bool ReleaseHelperHw<release>::isDotProductAccumulateSystolicSupported() const {
    return false;
}

template <>
inline bool ReleaseHelperHw<release>::isBindlessAddressingDisabled() const {
    return false;
}

template <>
inline bool ReleaseHelperHw<release>::isGlobalBindlessAllocatorEnabled() const {
    return true;
}

template <>
const SizeToPreferredSlmValueArray &ReleaseHelperHw<release>::getSizeToPreferredSlmValue(bool isHeapless) const {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename XeHpgCoreFamily::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;
    static const SizeToPreferredSlmValueArray sizeToPreferredSlmValue = {{
        {0, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0KB},
        {16 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_16KB},
        {32 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_32KB},
        {64 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_64KB},
        {std::numeric_limits<uint32_t>::max(), PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_96KB},
    }};
    return sizeToPreferredSlmValue;
}

template <>
bool ReleaseHelperHw<release>::isDummyBlitWaRequired() const {
    return true;
}

} // namespace NEO

#include "shared/source/release_helper/release_helper_common_xe_lpg.inl"

template class NEO::ReleaseHelperHw<NEO::release>;
