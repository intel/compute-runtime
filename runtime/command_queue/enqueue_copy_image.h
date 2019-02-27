/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/built_ins/built_ins.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/helpers/mipmap.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/surface.h"

#include "hw_cmds.h"

#include <algorithm>
#include <new>

namespace OCLRT {

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

    auto &builder = getDevice().getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyImageToImage3d,
                                                                                                        this->getContext(), this->getDevice());
    BuiltInOwnershipWrapper builtInLock(builder, this->context);

    MemObjSurface srcImgSurf(srcImage);
    MemObjSurface dstImgSurf(dstImage);
    Surface *surfaces[] = {&srcImgSurf, &dstImgSurf};

    BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
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
} // namespace OCLRT
