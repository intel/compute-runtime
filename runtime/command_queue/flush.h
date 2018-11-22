/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace OCLRT {
template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::flush() {
    getCommandStreamReceiver().flushBatchedSubmissions();
    return CL_SUCCESS;
}
} // namespace OCLRT
