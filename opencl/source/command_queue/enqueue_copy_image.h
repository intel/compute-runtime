/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/basic_math.h"
#include "opencl/source/built_ins/built_ins.h"
#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/helpers/mipmap.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/memory_manager/mem_obj_surface.h"

#include <algorithm>
#include <new>

namespace NEO {

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueCopyImage(
    Image *srcImage,
    Image *dstImage,
    const size_t srcOrigin[3],
    const size_t dstOrigin[3],
    const size_t region[3],
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {

    MultiDispatchInfo di;

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyImageToImage3d,
                                                                            this->getDevice());
    BuiltInOwnershipWrapper builtInLock(builder, this->context);

    MemObjSurface srcImgSurf(srcImage);
    MemObjSurface dstImgSurf(dstImage);
    Surface *surfaces[] = {&srcImgSurf, &dstImgSurf};

    BuiltinOpParams dc;
    dc.srcMemObj = srcImage;
    dc.dstMemObj = dstImage;
    dc.srcOffset = srcOrigin;
    dc.dstOffset = dstOrigin;
    dc.size = region;
    if (srcImage->getImageDesc().num_mip_levels > 0) {
        dc.srcMipLevel = findMipLevel(srcImage->getImageDesc().image_type, srcOrigin);
    }
    if (dstImage->getImageDesc().num_mip_levels > 0) {
        dc.dstMipLevel = findMipLevel(dstImage->getImageDesc().image_type, dstOrigin);
    }
    builder.buildDispatchInfos(di, dc);

    enqueueHandler<CL_COMMAND_COPY_IMAGE>(
        surfaces,
        false,
        di,
        numEventsInWaitList,
        eventWaitList,
        event);

    return CL_SUCCESS;
}
} // namespace NEO
