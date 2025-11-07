/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"

namespace NEO {
template <>
void ReleaseHelperHw<release>::adjustRTDispatchGlobals(void *rtDispatchGlobals, uint32_t rtStacksPerDss, bool heaplessEnabled, uint32_t maxBvhLevels) const {

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

    if (!heaplessEnabled) {
        return;
    }

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

} // namespace NEO
