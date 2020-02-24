/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/event/event.h"
#include "opencl/source/memory_manager/mem_obj_surface.h"

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
