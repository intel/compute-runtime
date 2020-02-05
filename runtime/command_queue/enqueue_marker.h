/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/device/device.h"
#include "runtime/event/event.h"
#include "runtime/memory_manager/mem_obj_surface.h"

#include <new>

namespace NEO {

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueMarkerWithWaitList(
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {

    NullSurface s;
    Surface *surfaces[] = {&s};
    enqueueHandler<CL_COMMAND_MARKER>(surfaces,
                                      false,
                                      MultiDispatchInfo(),
                                      numEventsInWaitList,
                                      eventWaitList,
                                      event);
    return CL_SUCCESS;
}
} // namespace NEO
