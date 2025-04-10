/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/basic_math.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/memory_manager/mem_obj_surface.h"

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

    auto builtInTypeImage3d = EBuiltInOps::adjustImageBuiltinType<EBuiltInOps::fillImage3d>(this->heaplessModeEnabled);
    auto builtInTypeImage1dBuffer = EBuiltInOps::adjustImageBuiltinType<EBuiltInOps::fillImage1dBuffer>(this->heaplessModeEnabled);

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(
        image->getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D_BUFFER
            ? builtInTypeImage1dBuffer
            : builtInTypeImage3d,
        this->getClDevice());

    BuiltInOwnershipWrapper builtInLock(builder, this->context);

    MemObjSurface dstImgSurf(image);
    Surface *surfaces[] = {&dstImgSurf};

    BuiltinOpParams dc;
    dc.srcPtr = const_cast<void *>(fillColor);
    dc.dstMemObj = image;
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = origin;
    dc.size = region;

    MultiDispatchInfo di(dc);

    builder.buildDispatchInfos(di);

    return enqueueHandler<CL_COMMAND_FILL_IMAGE>(
        surfaces,
        false,
        di,
        numEventsInWaitList,
        eventWaitList,
        event);
}
} // namespace NEO
