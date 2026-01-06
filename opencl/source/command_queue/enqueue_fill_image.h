/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/vec.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/memory_manager/mem_obj_surface.h"

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
