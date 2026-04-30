/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"

namespace NEO {
template <>
const SizeToPreferredSlmValueArray &ReleaseHelperHw<release>::getSizeToPreferredSlmValue() const {
    using PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_2 = typename Xe3pCoreFamily::INTERFACE_DESCRIPTOR_DATA_2::PREFERRED_SLM_ALLOCATION_SIZE;

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
    return sizeToPreferredSlmValueIdd2;
}

template <>
uint32_t ReleaseHelperHw<release>::alignSlmSize(uint32_t slmSize) const {
    static constexpr uint32_t largeSizes[] = {
        24u * MemoryConstants::kiloByte,
        32u * MemoryConstants::kiloByte,
        48u * MemoryConstants::kiloByte,
        64u * MemoryConstants::kiloByte,
        96u * MemoryConstants::kiloByte,
        128u * MemoryConstants::kiloByte,
        192u * MemoryConstants::kiloByte,
        256u * MemoryConstants::kiloByte,
        320u * MemoryConstants::kiloByte,
        384u * MemoryConstants::kiloByte,
    };

    if (slmSize <= 16u * MemoryConstants::kiloByte) {
        return alignUp(slmSize, MemoryConstants::kiloByte);
    }

    for (auto &alignedSlmSize : largeSizes) {
        if (slmSize <= alignedSlmSize) {
            return alignedSlmSize;
        }
    }

    UNRECOVERABLE_IF(true);
    return 0;
}

} // namespace NEO
