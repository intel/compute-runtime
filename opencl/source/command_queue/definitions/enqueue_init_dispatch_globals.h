/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/command_queue/command_queue_hw.h"

namespace NEO {

struct DispatchGlobalsArgs {
};

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueInitDispatchGlobals(DispatchGlobalsArgs *dispatchGlobalsArgs,
                                                             cl_uint numEventsInWaitList,
                                                             const cl_event *eventWaitList,
                                                             cl_event *event) {
    return CL_INVALID_VALUE;
}
} // namespace NEO
