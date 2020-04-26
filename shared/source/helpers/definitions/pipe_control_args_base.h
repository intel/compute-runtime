/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
namespace NEO {
struct PipeControlArgsBase {
    PipeControlArgsBase() = default;
    PipeControlArgsBase(bool dcFlush) : dcFlushEnable(dcFlush) {}

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
};
} // namespace NEO
