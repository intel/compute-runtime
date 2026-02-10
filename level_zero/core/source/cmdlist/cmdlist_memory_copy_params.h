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
struct SvmAllocationData;
} // namespace NEO

namespace L0 {

// Caches allocations found during obtainAllocData
// to avoid redundant lookups in getAlignedAllocationData
struct MemAllocInfo {
    NEO::SvmAllocationData *svmAlloc{nullptr};
    NEO::GraphicsAllocation *importedHostAlloc{nullptr};
    NEO::GraphicsAllocation *cachedHostAlloc{nullptr};
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
