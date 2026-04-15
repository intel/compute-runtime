/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/addressing_mode_helper.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/helpers/mipmap.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/memory_manager/mem_obj_surface.h"

namespace NEO {

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueCopyBufferToImage(
    Buffer *srcBuffer,
    Image *dstImage,
    size_t srcOffset,
    const size_t *dstOrigin,
    const size_t *region,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {

    auto builtInMode = this->defaultBuiltInMode;
    builtInMode.adjustToWideStatelessIfRequired(srcBuffer->getSize());
    BuiltIn::BuiltInId builtIn{BuiltIn::BaseKernel::copyBufferToImage3d, builtInMode};

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtIn,
                                                                              this->getClDevice());
    BuiltIn::OwnershipWrapper builtInLock(builder, this->context);

    MemObjSurface srcBufferSurf(srcBuffer);
    MemObjSurface dstImgSurf(dstImage);
    Surface *surfaces[] = {&srcBufferSurf, &dstImgSurf};

    BuiltIn::OpParams dc;
    dc.srcMemObj = srcBuffer;
    dc.dstMemObj = dstImage;
    dc.srcOffset = {srcOffset, 0, 0};
    dc.dstOffset = dstOrigin;
    dc.size = region;
    if (isMipMapped(dstImage->getImageDesc())) {
        dc.dstMipLevel = findMipLevel(dstImage->getImageDesc().image_type, dstOrigin);
    }

    MultiDispatchInfo dispatchInfo(dc);
    builder.buildDispatchInfos(dispatchInfo);

    return enqueueHandler<CL_COMMAND_COPY_BUFFER_TO_IMAGE>(
        surfaces,
        false,
        dispatchInfo,
        numEventsInWaitList,
        eventWaitList,
        event);
}

} // namespace NEO
