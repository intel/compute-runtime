/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/helpers/mipmap.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/memory_manager/mem_obj_surface.h"

namespace NEO {

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueCopyImageToBuffer(
    Image *srcImage,
    Buffer *dstBuffer,
    const size_t *srcOrigin,
    const size_t *region,
    size_t dstOffset,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {

    bool isStateless = isForceStateless;
    if (dstBuffer->getSize() >= 4ull * MemoryConstants::gigaByte) {
        isStateless = true;
    }
    auto builtInType = EBuiltInOps::adjustBuiltinType<EBuiltInOps::copyImage3dToBuffer>(isStateless, this->heaplessModeEnabled);
    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInType,
                                                                            this->getClDevice());
    BuiltInOwnershipWrapper builtInLock(builder, this->context);

    MemObjSurface srcImgSurf(srcImage);
    MemObjSurface dstBufferSurf(dstBuffer);
    Surface *surfaces[] = {&srcImgSurf, &dstBufferSurf};

    BuiltinOpParams dc;
    dc.srcMemObj = srcImage;
    dc.dstMemObj = dstBuffer;
    dc.srcOffset = srcOrigin;
    dc.dstOffset = {dstOffset, 0, 0};
    dc.size = region;
    if (isMipMapped(srcImage->getImageDesc())) {
        dc.srcMipLevel = findMipLevel(srcImage->getImageDesc().image_type, srcOrigin);
    }

    MultiDispatchInfo dispatchInfo(dc);
    builder.buildDispatchInfos(dispatchInfo);

    return enqueueHandler<CL_COMMAND_COPY_IMAGE_TO_BUFFER>(
        surfaces,
        false,
        dispatchInfo,
        numEventsInWaitList,
        eventWaitList,
        event);
}

} // namespace NEO
