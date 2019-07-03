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
#include "runtime/context/context.h"
#include "runtime/event/event.h"
#include "runtime/helpers/cache_policy.h"
#include "runtime/helpers/hardware_commands_helper.h"
#include "runtime/helpers/mipmap.h"
#include "runtime/helpers/surface_formats.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/surface.h"

#include "hw_cmds.h"

#include <algorithm>
#include <new>

namespace NEO {

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueReadImage(
    Image *srcImage,
    cl_bool blockingRead,
    const size_t *origin,
    const size_t *region,
    size_t inputRowPitch,
    size_t inputSlicePitch,
    void *ptr,
    GraphicsAllocation *mapAllocation,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {

    if (nullptr == mapAllocation) {
        notifyEnqueueReadImage(srcImage, !!blockingRead);
    }

    MultiDispatchInfo di;
    auto isMemTransferNeeded = true;
    if (srcImage->isMemObjZeroCopy()) {
        size_t hostOffset;
        Image::calculateHostPtrOffset(&hostOffset, origin, region, inputRowPitch, inputSlicePitch, srcImage->getImageDesc().image_type, srcImage->getSurfaceFormatInfo().ImageElementSizeInBytes);
        isMemTransferNeeded = srcImage->checkIfMemoryTransferIsRequired(hostOffset, 0, ptr, CL_COMMAND_READ_IMAGE);
    }
    if (!isMemTransferNeeded) {
        return enqueueMarkerForReadWriteOperation(srcImage, ptr, CL_COMMAND_READ_IMAGE, blockingRead,
                                                  numEventsInWaitList, eventWaitList, event);
    }

    auto &builder = getDevice().getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyImage3dToBuffer,
                                                                                                        this->getContext(), this->getDevice());

    BuiltInOwnershipWrapper builtInLock(builder, this->context);

    size_t hostPtrSize = calculateHostPtrSizeForImage(region, inputRowPitch, inputSlicePitch, srcImage);
    void *dstPtr = ptr;

    MemObjSurface srcImgSurf(srcImage);
    HostPtrSurface hostPtrSurf(dstPtr, hostPtrSize);
    GeneralSurface mapSurface;
    Surface *surfaces[] = {&srcImgSurf, nullptr};

    if (mapAllocation) {
        surfaces[1] = &mapSurface;
        mapSurface.setGraphicsAllocation(mapAllocation);
        //get offset between base cpu ptr of map allocation and dst ptr
        size_t dstOffset = ptrDiff(dstPtr, mapAllocation->getUnderlyingBuffer());
        dstPtr = reinterpret_cast<void *>(mapAllocation->getGpuAddress() + dstOffset);
    } else {
        surfaces[1] = &hostPtrSurf;
        if (region[0] != 0 &&
            region[1] != 0 &&
            region[2] != 0) {
            bool status = getCommandStreamReceiver().createAllocationForHostSurface(hostPtrSurf, true);
            if (!status) {
                return CL_OUT_OF_RESOURCES;
            }
            dstPtr = reinterpret_cast<void *>(hostPtrSurf.getAllocation()->getGpuAddress());
        }
    }

    void *alignedDstPtr = alignDown(dstPtr, 4);
    size_t dstPtrOffset = ptrDiff(dstPtr, alignedDstPtr);

    BuiltinOpParams dc;
    dc.srcMemObj = srcImage;
    dc.dstPtr = alignedDstPtr;
    dc.dstOffset.x = dstPtrOffset;
    dc.srcOffset = origin;
    dc.size = region;
    dc.srcRowPitch = (srcImage->getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY) ? inputSlicePitch : inputRowPitch;
    dc.srcSlicePitch = inputSlicePitch;
    if (srcImage->getImageDesc().num_mip_levels > 0) {
        dc.srcMipLevel = findMipLevel(srcImage->getImageDesc().image_type, origin);
    }
    builder.buildDispatchInfos(di, dc);

    enqueueHandler<CL_COMMAND_READ_IMAGE>(
        surfaces,
        blockingRead == CL_TRUE,
        di,
        numEventsInWaitList,
        eventWaitList,
        event);

    if (context->isProvidingPerformanceHints()) {
        if (!isL3Capable(ptr, hostPtrSize)) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, CL_ENQUEUE_READ_IMAGE_DOESNT_MEET_ALIGNMENT_RESTRICTIONS, ptr, hostPtrSize, MemoryConstants::pageSize, MemoryConstants::pageSize);
        }
    }

    return CL_SUCCESS;
}
} // namespace NEO
