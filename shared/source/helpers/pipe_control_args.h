/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

struct PipeControlArgs {
    PipeControlArgs() = default;

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
};

} // namespace NEO
