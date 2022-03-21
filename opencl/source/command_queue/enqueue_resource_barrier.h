/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/event/event.h"
#include "opencl/source/memory_manager/resource_surface.h"

#include "resource_barrier.h"

#include <new>

namespace NEO {

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueResourceBarrier(BarrierCommand *resourceBarrier,
                                                         cl_uint numEventsInWaitList,
                                                         const cl_event *eventWaitList,
                                                         cl_event *event) {
    MultiDispatchInfo multiDispatch;
    return enqueueHandler<CL_COMMAND_RESOURCE_BARRIER>(resourceBarrier->surfacePtrs.begin(),
                                                       resourceBarrier->numSurfaces,
                                                       false,
                                                       multiDispatch,
                                                       numEventsInWaitList,
                                                       eventWaitList,
                                                       event);
}

} // namespace NEO
