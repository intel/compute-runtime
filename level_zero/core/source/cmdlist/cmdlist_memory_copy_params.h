/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "cmdlist_memory_copy_params_ext.h"

#include <cstddef>
#include <cstdint>

namespace NEO {
class GraphicsAllocation;
}

namespace L0 {

// Caches host pointer allocations found during obtainAllocData
// to avoid redundant lookups in getAlignedAllocationData
struct CachedHostPtrAllocs {
    NEO::GraphicsAllocation *srcAlloc{nullptr};
    NEO::GraphicsAllocation *dstAlloc{nullptr};

    CachedHostPtrAllocs() = default;
    CachedHostPtrAllocs(NEO::GraphicsAllocation *src, NEO::GraphicsAllocation *dst)
        : srcAlloc(src), dstAlloc(dst) {}
};

struct CmdListMemoryCopyParams {
    uint64_t forceAggregatedEventIncValue = 0;
    const void *bcsSplitBaseSrcPtr = nullptr;
    void *bcsSplitBaseDstPtr = nullptr;
    size_t bcsSplitTotalSrcSize = 0;
    size_t bcsSplitTotalDstSize = 0;
    bool relaxedOrderingDispatch = false;
    bool forceDisableCopyOnlyInOrderSignaling = false;
    bool copyOffloadAllowed = false;
    bool taskCountUpdateRequired = false;
    bool bscSplitEnabled = false;
    CmdListMemoryCopyParamsExt paramsExt{};
};

} // namespace L0
