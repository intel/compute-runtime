/*
 * Copyright (C) 2017-2019 Intel Corporation
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
#include "runtime/helpers/mipmap.h"
#include "runtime/helpers/surface_formats.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/graphics_allocation.h"

#include "hw_cmds.h"

#include <algorithm>
#include <new>

namespace NEO {

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueWriteImage(
    Image *dstImage,
    cl_bool blockingWrite,
    const size_t *origin,
    const size_t *region,
    size_t inputRowPitch,
    size_t inputSlicePitch,
    const void *ptr,
    GraphicsAllocation *mapAllocation,
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
        return enqueueMarkerForReadWriteOperation(dstImage, const_cast<void *>(ptr), CL_COMMAND_WRITE_IMAGE, blockingWrite,
                                                  numEventsInWaitList, eventWaitList, event);
    }
    auto &builder = getDevice().getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToImage3d,
                                                                                                        this->getContext(), this->getDevice());

    BuiltInOwnershipWrapper lock(builder, this->context);

    size_t hostPtrSize = calculateHostPtrSizeForImage(region, inputRowPitch, inputSlicePitch, dstImage);
    void *srcPtr = const_cast<void *>(ptr);

    MemObjSurface dstImgSurf(dstImage);
    HostPtrSurface hostPtrSurf(srcPtr, hostPtrSize, true);
    GeneralSurface mapSurface;
    Surface *surfaces[] = {&dstImgSurf, nullptr};

    if (mapAllocation) {
        surfaces[1] = &mapSurface;
        mapSurface.setGraphicsAllocation(mapAllocation);
        //get offset between base cpu ptr of map allocation and dst ptr
        size_t srcOffset = ptrDiff(srcPtr, mapAllocation->getUnderlyingBuffer());
        srcPtr = reinterpret_cast<void *>(mapAllocation->getGpuAddress() + srcOffset);
    } else {
        surfaces[1] = &hostPtrSurf;
        if (region[0] != 0 &&
            region[1] != 0 &&
            region[2] != 0) {
            bool status = getCommandStreamReceiver().createAllocationForHostSurface(hostPtrSurf, false);
            if (!status) {
                return CL_OUT_OF_RESOURCES;
            }
            srcPtr = reinterpret_cast<void *>(hostPtrSurf.getAllocation()->getGpuAddress());
        }
    }

    void *alignedSrcPtr = alignDown(srcPtr, 4);
    size_t srcPtrOffset = ptrDiff(srcPtr, alignedSrcPtr);

    BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
    dc.srcPtr = alignedSrcPtr;
    dc.srcOffset.x = srcPtrOffset;
    dc.dstMemObj = dstImage;
    dc.dstOffset = origin;
    dc.size = region;
    dc.dstRowPitch = ((dstImage->getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY) && (inputSlicePitch > inputRowPitch)) ? inputSlicePitch : inputRowPitch;
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
} // namespace NEO
