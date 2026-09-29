/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/cmdqueue/patch_preamble_data_types.h"

#include <vector>

namespace L0 {
struct CommandList;

struct PatchPreambleCountersCrossSync {
    PatchPreambleCounter counter = 0;
    PatchPreambleDeviceGpuAddress deviceGpuAddress = 0;
    PatchPreambleDeviceGraphicsAllocation deviceGraphicsAllocation = nullptr;

    CommandList *appendedCommandListToSyncBefore = nullptr;
};

using PatchPreambleCountersCrossSyncList = std::vector<PatchPreambleCountersCrossSync>;

struct PatchPreambleCountersCrossSyncContainer {
    PatchPreambleCountersCrossSyncList list;
};

} // namespace L0
