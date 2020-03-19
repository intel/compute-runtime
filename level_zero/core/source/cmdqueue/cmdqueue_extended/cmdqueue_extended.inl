/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.h"

namespace L0 {
template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandQueueHw<gfxCoreFamily>::executeCommands(uint32_t numCommandGraphs,
                                                           void *phCommands,
                                                           ze_fence_handle_t hFence) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
} // namespace L0
