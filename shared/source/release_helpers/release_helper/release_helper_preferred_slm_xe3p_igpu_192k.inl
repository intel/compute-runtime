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
        {std::numeric_limits<uint32_t>::max(), PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_2::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_192K},
    }};
    return sizeToPreferredSlmValueIdd2;
}

template <>
uint32_t ReleaseHelperHw<release>::computeSlmValues(uint32_t slmSize) const {
    using SHARED_LOCAL_MEMORY_SIZE = typename Xe3pCoreFamily::INTERFACE_DESCRIPTOR_DATA_2::SHARED_LOCAL_MEMORY_SIZE;

    UNRECOVERABLE_IF(slmSize > 192u * MemoryConstants::kiloByte);

    if (slmSize > 128u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_192K;
    }
    if (slmSize > 96u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_128K;
    }
    if (slmSize > 64u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_96K;
    }
    if (slmSize > 48u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_64K;
    }
    if (slmSize > 32u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_48K;
    }
    if (slmSize > 24u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_32K;
    }
    if (slmSize > 16u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_24K;
    }
    if (slmSize > 15u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_16K;
    }
    if (slmSize > 14u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_15K;
    }
    if (slmSize > 13u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_14K;
    }
    if (slmSize > 12u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_13K;
    }
    if (slmSize > 11u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_12K;
    }
    if (slmSize > 10u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_11K;
    }
    if (slmSize > 9u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_10K;
    }
    if (slmSize > 8u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_9K;
    }
    if (slmSize > 7u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_8K;
    }
    if (slmSize > 6u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_7K;
    }
    if (slmSize > 5u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_6K;
    }
    if (slmSize > 4u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_5K;
    }
    if (slmSize > 3u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_4K;
    }
    if (slmSize > 2u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_3K;
    }
    if (slmSize > 1u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_2K;
    }
    return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_1K;
}

template <>
uint32_t ReleaseHelperHw<release>::alignSlmSizePerThreadGroup(uint32_t slmSize) const {
    static constexpr uint32_t largeSizes[] = {
        24u * MemoryConstants::kiloByte,
        32u * MemoryConstants::kiloByte,
        48u * MemoryConstants::kiloByte,
        64u * MemoryConstants::kiloByte,
        96u * MemoryConstants::kiloByte,
        128u * MemoryConstants::kiloByte,
        192u * MemoryConstants::kiloByte,
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
