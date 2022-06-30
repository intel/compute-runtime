/*
 * Copyright (C) 2018-2022 Intel Corporation
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
    const size_t *srcOrigin,
    const size_t *dstOrigin,
    const size_t *region,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {
    constexpr cl_command_type cmdType = CL_COMMAND_COPY_IMAGE;

    CsrSelectionArgs csrSelectionArgs{cmdType, srcImage, dstImage, device->getRootDeviceIndex(), region, srcOrigin, dstOrigin};
    CommandStreamReceiver &csr = selectCsrForBuiltinOperation(csrSelectionArgs);

    MemObjSurface srcImgSurf(srcImage);
    MemObjSurface dstImgSurf(dstImage);
    Surface *surfaces[] = {&srcImgSurf, &dstImgSurf};

    BuiltinOpParams dc;
    dc.srcMemObj = srcImage;
    dc.dstMemObj = dstImage;
    dc.srcOffset = srcOrigin;
    dc.dstOffset = dstOrigin;
    dc.size = region;
    if (isMipMapped(srcImage->getImageDesc())) {
        dc.srcMipLevel = findMipLevel(srcImage->getImageDesc().image_type, srcOrigin);
    }
    if (isMipMapped(dstImage->getImageDesc())) {
        dc.dstMipLevel = findMipLevel(dstImage->getImageDesc().image_type, dstOrigin);
    }

    MultiDispatchInfo dispatchInfo(dc);

    return dispatchBcsOrGpgpuEnqueue<CL_COMMAND_COPY_IMAGE>(dispatchInfo, surfaces, EBuiltInOps::CopyImageToImage3d, numEventsInWaitList, eventWaitList, event, false, csr);
}

} // namespace NEO
