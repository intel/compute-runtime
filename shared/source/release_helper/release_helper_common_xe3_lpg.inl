/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/string.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/xe3_core/hw_cmds_base.h"

namespace NEO {

template <>
bool ReleaseHelperHw<release>::isDeviceConfigStringTileCountIncluded() const {
    return true;
}

template <>
const SizeToPreferredSlmValueArray &ReleaseHelperHw<release>::getSizeToPreferredSlmValue() const {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename Xe3CoreFamily::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;
    static const SizeToPreferredSlmValueArray sizeToPreferredSlmValue = {{
        {0, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_0K},
        {16 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_16K},
        {32 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_32K},
        {64 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_64K},
        {96 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_96K},
        {std::numeric_limits<uint32_t>::max(), PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_128K},
    }};
    return sizeToPreferredSlmValue;
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
