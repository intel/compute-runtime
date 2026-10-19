/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/cmdqueue/immediate_completion_cross_sync_definitions.h"
#include "level_zero/core/source/cmdqueue/patch_preamble_cross_sync_definitions.h"

namespace L0 {

// Barrier shared by all executors taking part in a single execution.
// Each executor increments the counter before its patch preamble (arrive) and after it (release),
// then waits until all executors reached the same point.
struct PatchPreambleBarrier {
    uint64_t counterGpuAddress = 0;
    NEO::GraphicsAllocation *counterAllocation = nullptr;
    uint64_t arriveValue = 0;
    uint64_t releaseValue = 0;
};

struct CountersCrossSyncContainer {
    PatchPreambleCountersCrossSyncList patchPreambleCrossSyncList;
    ImmediateCountersCrossSyncList immediateCompletionCrossSyncList;
    PatchPreambleBarrier barrier;

    bool isBarrierRequired() const {
        return barrier.counterAllocation != nullptr;
    }
};

} // namespace L0
