/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/string.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/release_helper/release_helper_base.inl"
#include "shared/source/release_helper/release_helper_common_xe3p.h"
#include "shared/source/xe3p_core/hw_cmds_base.h"

#include "release_definitions.h"

namespace NEO {
constexpr auto release = ReleaseType::release3511;
template <>
bool ReleaseHelperHw<release>::isBFloat16ConversionSupported() const {
    return true;
}

template <>
std::vector<uint32_t> ReleaseHelperHw<release>::getSupportedNumGrfs() const {
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
const SizeToPreferredSlmValueArray &ReleaseHelperHw<release>::getSizeToPreferredSlmValue(bool isHeapless) const {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename Xe3pCoreFamily::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;
    using PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_2 = typename Xe3pCoreFamily::INTERFACE_DESCRIPTOR_DATA_2::PREFERRED_SLM_ALLOCATION_SIZE;
    static const SizeToPreferredSlmValueArray sizeToPreferredSlmValueIdd = {{
        {0, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_0K},
        {16 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_16K},
        {32 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_32K},
        {64 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_64K},
        {96 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_96K},
        {128 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_128K},
        {160 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_160K},
        {192 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_192K},
        {256 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_256K},
        {320 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_320K},
        {std::numeric_limits<uint32_t>::max(), PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_384K},
    }};
    static const SizeToPreferredSlmValueArray sizeToPreferredSlmValueIdd2 = {{
        {0, PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_2::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_0K},
        {16 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_2::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_16K},
        {32 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_2::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_32K},
        {64 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_2::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_64K},
        {96 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_2::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_96K},
        {128 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_2::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_128K},
        {160 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_2::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_160K},
        {192 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_2::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_192K},
        {256 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_2::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_256K},
        {320 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_2::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_320K},
        {std::numeric_limits<uint32_t>::max(), PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_2::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_384K},
    }};
    if (isHeapless) {
        return sizeToPreferredSlmValueIdd2;
    } else {
        return sizeToPreferredSlmValueIdd;
    }
}

template <>
bool ReleaseHelperHw<release>::getFtrXe2Compression() const {
    return false;
}

template <>
uint32_t ReleaseHelperHw<release>::computeSlmValues(uint32_t slmSize, bool isHeapless) const {
    if (isHeapless) {
        return computeSlmValuesXe3pHeapless(slmSize);
    }
    return computeSlmValuesXe3p(slmSize);
}

} // namespace NEO

#include "shared/source/release_helper/release_helper_common_xe3_and_later.inl"
#include "shared/source/release_helper/release_helper_common_xe3p.inl"
template class NEO::ReleaseHelperHw<NEO::release>;
