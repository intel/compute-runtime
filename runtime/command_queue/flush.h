/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::flush() {
    return getGpgpuCommandStreamReceiver().flushBatchedSubmissions() ? CL_SUCCESS : CL_OUT_OF_RESOURCES;
}
} // namespace NEO
