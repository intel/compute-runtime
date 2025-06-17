/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

namespace NEO {

template <>
bool ReleaseHelperHw<release>::isProgramAllStateComputeCommandFieldsWARequired() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::isSplitMatrixMultiplyAccumulateSupported() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::isBFloat16ConversionSupported() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::isDirectSubmissionSupported() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::isRcsExposureDisabled() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::isBindlessAddressingDisabled() const {
    return false;
}

template <>
bool ReleaseHelperHw<release>::isGlobalBindlessAllocatorEnabled() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::getFtrXe2Compression() const {
    return false;
}

template <>
const SizeToPreferredSlmValueArray &ReleaseHelperHw<release>::getSizeToPreferredSlmValue(bool isHeapless) const {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename XeHpgCoreFamily::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;
    static const SizeToPreferredSlmValueArray sizeToPreferredSlmValue = {{
        {0, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0KB},
        {16 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_16KB},
        {32 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_32KB},
        {64 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_64KB},
        {96 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_96KB},
        {std::numeric_limits<uint32_t>::max(), PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_128KB},
    }};
    return sizeToPreferredSlmValue;
}
template <>
bool ReleaseHelperHw<release>::isPostImageWriteFlushRequired() const {
    return true;
}
} // namespace NEO
