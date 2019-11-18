/*
 * Copyright (C) 2017-2019 Intel Corporation
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
#include "runtime/memory_manager/surface.h"

namespace NEO {

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueMigrateMemObjects(cl_uint numMemObjects,
                                                           const cl_mem *memObjects,
                                                           cl_mem_migration_flags flags,
                                                           cl_uint numEventsInWaitList,
                                                           const cl_event *eventWaitList,
                                                           cl_event *event) {
    NullSurface s;
    Surface *surfaces[] = {&s};

    enqueueHandler<CL_COMMAND_MIGRATE_MEM_OBJECTS>(surfaces,
                                                   false,
                                                   MultiDispatchInfo(),
                                                   numEventsInWaitList,
                                                   eventWaitList,
                                                   event);

    return CL_SUCCESS;
}
} // namespace NEO
