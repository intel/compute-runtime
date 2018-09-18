/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/built_ins/built_ins.h"
#include "hw_cmds.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/surface_formats.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/helpers/mipmap.h"
#include "runtime/mem_obj/image.h"
#include <algorithm>
#include <new>

namespace OCLRT {

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueWriteImage(
    Image *dstImage,
    cl_bool blockingWrite,
    const size_t *origin,
    const size_t *region,
    size_t inputRowPitch,
    size_t inputSlicePitch,
    const void *ptr,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {

    MultiDispatchInfo di;
    auto isMemTransferNeeded = true;
    if (dstImage->isMemObjZeroCopy()) {
        size_t hostOffset;
        Image::calculateHostPtrOffset(&hostOffset, origin, region, inputRowPitch, inputSlicePitch, dstImage->getImageDesc().image_type, dstImage->getSurfaceFormatInfo().ImageElementSizeInBytes);
        isMemTransferNeeded = dstImage->checkIfMemoryTransferIsRequired(hostOffset, 0, ptr, CL_COMMAND_WRITE_IMAGE);
    }
    if (!isMemTransferNeeded) {
        NullSurface s;
        Surface *surfaces[] = {&s};
        enqueueHandler<CL_COMMAND_MARKER>(
            surfaces,
            blockingWrite == CL_TRUE,
            di,
            numEventsInWaitList,
            eventWaitList,
            event);
        if (event) {
            auto pEvent = castToObjectOrAbort<Event>(*event);
            pEvent->setCmdType(CL_COMMAND_WRITE_IMAGE);
        }

        if (context->isProvidingPerformanceHints()) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, CL_ENQUEUE_WRITE_IMAGE_DOESNT_REQUIRES_COPY_DATA, static_cast<cl_mem>(dstImage));
        }

        return CL_SUCCESS;
    }
    auto &builder = getDevice().getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToImage3d,
                                                                                                        this->getContext(), this->getDevice());

    BuiltInOwnershipWrapper lock(builder, this->context);

    size_t hostPtrSize = calculateHostPtrSizeForImage(const_cast<size_t *>(region), inputRowPitch, inputSlicePitch, dstImage);
    void *srcPtr = const_cast<void *>(ptr);

    MemObjSurface dstImgSurf(dstImage);
    HostPtrSurface hostPtrSurf(srcPtr, hostPtrSize, true);
    Surface *surfaces[] = {&dstImgSurf, &hostPtrSurf};

    if (region[0] != 0 &&
        region[1] != 0 &&
        region[2] != 0) {
        bool status = createAllocationForHostSurface(hostPtrSurf);
        if (!status) {
            return CL_OUT_OF_RESOURCES;
        }
        srcPtr = reinterpret_cast<void *>(hostPtrSurf.getAllocation()->getGpuAddressToPatch());
    }

    BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
    dc.srcPtr = srcPtr;
    dc.dstMemObj = dstImage;
    dc.dstOffset = origin;
    dc.size = region;
    dc.dstRowPitch = inputRowPitch;
    dc.dstSlicePitch = inputSlicePitch;
    if (dstImage->getImageDesc().num_mip_levels > 0) {
        dc.dstMipLevel = findMipLevel(dstImage->getImageDesc().image_type, origin);
    }

    builder.buildDispatchInfos(di, dc);

    enqueueHandler<CL_COMMAND_WRITE_IMAGE>(
        surfaces,
        blockingWrite == CL_TRUE,
        di,
        numEventsInWaitList,
        eventWaitList,
        event);

    if (context->isProvidingPerformanceHints()) {
        context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_NEUTRAL_INTEL, CL_ENQUEUE_WRITE_IMAGE_REQUIRES_COPY_DATA, static_cast<cl_mem>(dstImage));
    }

    return CL_SUCCESS;
}
} // namespace OCLRT
