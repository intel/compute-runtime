/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
namespace NEO {
struct PipeControlArgsBase {
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

  protected:
    PipeControlArgsBase() = default;
    PipeControlArgsBase(bool dcFlush) : dcFlushEnable(dcFlush) {}
};
} // namespace NEO
