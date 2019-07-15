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
    getGpgpuCommandStreamReceiver().flushBatchedSubmissions();
    return CL_SUCCESS;
}
} // namespace NEO
