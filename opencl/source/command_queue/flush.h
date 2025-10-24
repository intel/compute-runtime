/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/command_queue/command_queue_hw.h"

namespace NEO {
template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::flush() {
    return getGpgpuCommandStreamReceiver().flushBatchedSubmissions() ? CL_SUCCESS : CL_OUT_OF_RESOURCES;
}
} // namespace NEO
