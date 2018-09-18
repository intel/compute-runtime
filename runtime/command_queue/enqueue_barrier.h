/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "hw_cmds.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/device/device.h"
#include "runtime/event/event.h"
#include "runtime/memory_manager/surface.h"
#include <new>

namespace OCLRT {

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueBarrierWithWaitList(
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {
    NullSurface s;
    Surface *surfaces[] = {&s};
    cl_uint dimensions = 1;
    enqueueHandler<CL_COMMAND_BARRIER>(surfaces,
                                       false,
                                       nullptr,
                                       dimensions,
                                       nullptr,
                                       nullptr,
                                       nullptr,
                                       numEventsInWaitList,
                                       eventWaitList,
                                       event);

    return CL_SUCCESS;
}
} // namespace OCLRT
