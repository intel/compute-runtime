/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

struct PipeControlArgs {
    PipeControlArgs() = default;
    void *postSyncCmd = nullptr;

    bool blockSettingPostSyncProperties = false;
    bool csStallOnly = false;
    bool dcFlushEnable = false;
    bool renderTargetCacheFlushEnable = false;
    bool instructionCacheInvalidateEnable = false;
    bool textureCacheInvalidationEnable = false;
    bool pipeControlFlushEnable = false;
    bool vfCacheInvalidationEnable = false;
    bool constantCacheInvalidationEnable = false;
    bool stateCacheInvalidationEnable = false;
    bool genericMediaStateClear = false;
    bool hdcPipelineFlush = false;
    bool tlbInvalidation = false;
    bool compressionControlSurfaceCcsFlush = false;
    bool notifyEnable = false;
    bool workloadPartitionOffset = false;
    bool amfsFlushEnable = false;
    bool unTypedDataPortCacheFlush = false;
    bool depthCacheFlushEnable = false;
    bool depthStallEnable = false;
    bool protectedMemoryDisable = false;
    bool isWalkerWithProfilingEnqueued = false;
    bool commandCacheInvalidateEnable = false;
    bool isL1InvalidateRequired = false;
    bool isL1FlushRequired = false;
};

} // namespace NEO
