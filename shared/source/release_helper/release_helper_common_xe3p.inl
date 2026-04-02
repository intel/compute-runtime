/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"

namespace NEO {
template <>
void ReleaseHelperHw<release>::adjustRTDispatchGlobals(void *rtDispatchGlobals, uint32_t rtStacksPerDss, uint32_t maxBvhLevels) const {

    struct alignas(32) RTDispatchGlobals2 {
        // Cached by HW
      public:
        uint64_t rtMemBasePtr;            // base address of the allocated stack memory
        uint64_t callStackHandlerKSP;     // this is the KSP of the continuation handler that is invoked by BTD when the read KSP is 0
        uint32_t stackSizePerRay;         // maximal stack size of a ray in 64 byte blocks
        uint32_t numDSSRTStacks : 16;     // number of stacks per DSS
        uint32_t syncNumDSSRTStacks : 16; // number of synch stacks per DSS
        uint32_t maxBVHLevels : 3;        // the maximal number of supported instancing levels, 0->8, 1->1, 2->2, etc.
        uint32_t padding1 : 29;           // padding to 32 bytes

        uint32_t flags : 1;
        uint32_t padding2 : 31;
        // Not cached by HW
      public:
        uint64_t hitGroupBasePtr;           // base pointer of hit group shader record array (16-bytes alignment)
        uint64_t missShaderBasePtr;         // base pointer of miss shader record array (8-bytes alignment)
        uint64_t callableShaderBasePtr;     // base pointer of callable shader record array (8-bytes alignment)
        uint32_t hitGroupStride;            // stride of hit group shader records (16-bytes alignment)
        uint32_t missShaderStride;          // stride of miss shader records (8-bytes alignment)
        uint32_t callableShaderStride;      // stride of callable shader records (8-bytes alignment)
        uint32_t dispatchRaysDimensions[3]; // dispatch dimensions of the thread grid
    };

    constexpr uint32_t maxNumDSSRTStacks = 2048u;
    constexpr uint32_t maxSyncNumDSSRTStacks = 4096u;

    auto *dispatchGlobals = reinterpret_cast<RTDispatchGlobals2 *>(rtDispatchGlobals);
    dispatchGlobals->numDSSRTStacks = std::min(rtStacksPerDss, maxNumDSSRTStacks);
    dispatchGlobals->syncNumDSSRTStacks = std::min(rtStacksPerDss, maxSyncNumDSSRTStacks);
    dispatchGlobals->flags = 1;

    if (maxBvhLevels >= 8) {
        maxBvhLevels = 0;
    }
    dispatchGlobals->maxBVHLevels = maxBvhLevels;
}

template <>
bool ReleaseHelperHw<release>::isResolvingSubDeviceIDNeeded() const {
    return false;
}

template <>
uint32_t ReleaseHelperHw<release>::computeSlmValues(uint32_t slmSize) const {
    using SHARED_LOCAL_MEMORY_SIZE = typename Xe3pCoreFamily::INTERFACE_DESCRIPTOR_DATA_2::SHARED_LOCAL_MEMORY_SIZE;

    UNRECOVERABLE_IF(slmSize > 384u * MemoryConstants::kiloByte);

    if (slmSize > 320u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_384K;
    }
    if (slmSize > 256u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_320K;
    }
    if (slmSize > 192u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_256K;
    }
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

} // namespace NEO
