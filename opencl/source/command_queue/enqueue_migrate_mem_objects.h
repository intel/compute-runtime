/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/surface.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/helpers/dispatch_info.h"

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

    return enqueueHandler<CL_COMMAND_MIGRATE_MEM_OBJECTS>(surfaces,
                                                          false,
                                                          MultiDispatchInfo(),
                                                          numEventsInWaitList,
                                                          eventWaitList,
                                                          event);
}

} // namespace NEO
