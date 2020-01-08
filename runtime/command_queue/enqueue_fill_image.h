/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/basic_math.h"
#include "runtime/built_ins/built_ins.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/hardware_commands_helper.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/surface.h"

#include <algorithm>
#include <new>

namespace NEO {

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueFillImage(
    Image *image,
    const void *fillColor,
    const size_t *origin,
    const size_t *region,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {

    MultiDispatchInfo di;

    auto &builder = getDevice().getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::FillImage3d,
                                                                                                        this->getContext(), this->getDevice());
    BuiltInOwnershipWrapper builtInLock(builder, this->context);

    MemObjSurface dstImgSurf(image);
    Surface *surfaces[] = {&dstImgSurf};

    BuiltinOpParams dc;
    dc.srcPtr = const_cast<void *>(fillColor);
    dc.dstMemObj = image;
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = origin;
    dc.size = region;
    builder.buildDispatchInfos(di, dc);

    enqueueHandler<CL_COMMAND_FILL_IMAGE>(
        surfaces,
        false,
        di,
        numEventsInWaitList,
        eventWaitList,
        event);

    return CL_SUCCESS;
}
} // namespace NEO
