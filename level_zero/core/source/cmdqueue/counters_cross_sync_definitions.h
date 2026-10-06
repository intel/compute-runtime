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

struct CountersCrossSyncContainer {
    PatchPreambleCountersCrossSyncList patchPreambleCrossSyncList;
    ImmediateCountersCrossSyncList immediateCompletionCrossSyncList;
};

} // namespace L0
