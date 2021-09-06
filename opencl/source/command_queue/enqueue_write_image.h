/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/memory_manager/graphics_allocation.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/helpers/mipmap.h"
#include "opencl/source/mem_obj/image.h"

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
    constexpr cl_command_type cmdType = CL_COMMAND_WRITE_IMAGE;

    CsrSelectionArgs csrSelectionArgs{cmdType, nullptr, dstImage, device->getRootDeviceIndex(), region, nullptr, origin};
    CommandStreamReceiver &csr = selectCsrForBuiltinOperation(csrSelectionArgs);

    auto isMemTransferNeeded = true;

    if (dstImage->isMemObjZeroCopy()) {
        size_t hostOffset;
        Image::calculateHostPtrOffset(&hostOffset, origin, region, inputRowPitch, inputSlicePitch, dstImage->getImageDesc().image_type, dstImage->getSurfaceFormatInfo().surfaceFormat.ImageElementSizeInBytes);
        isMemTransferNeeded = dstImage->checkIfMemoryTransferIsRequired(hostOffset, 0, ptr, cmdType);
    }
    if (!isMemTransferNeeded) {
        return enqueueMarkerForReadWriteOperation(dstImage, const_cast<void *>(ptr), cmdType, blockingWrite,
                                                  numEventsInWaitList, eventWaitList, event);
    }

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
            bool status = csr.createAllocationForHostSurface(hostPtrSurf, false);
            if (!status) {
                return CL_OUT_OF_RESOURCES;
            }
            srcPtr = reinterpret_cast<void *>(hostPtrSurf.getAllocation()->getGpuAddress());
        }
    }

    void *alignedSrcPtr = alignDown(srcPtr, 4);
    size_t srcPtrOffset = ptrDiff(srcPtr, alignedSrcPtr);

    BuiltinOpParams dc;
    dc.srcPtr = alignedSrcPtr;
    dc.srcOffset.x = srcPtrOffset;
    dc.dstMemObj = dstImage;
    dc.dstOffset = origin;
    dc.size = region;
    dc.srcRowPitch = ((dstImage->getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY) && (inputSlicePitch > inputRowPitch)) ? inputSlicePitch : inputRowPitch;
    dc.srcSlicePitch = inputSlicePitch;
    if (isMipMapped(dstImage->getImageDesc())) {
        dc.dstMipLevel = findMipLevel(dstImage->getImageDesc().image_type, origin);
    }
    dc.transferAllocation = mapAllocation ? mapAllocation : hostPtrSurf.getAllocation();

    auto eBuiltInOps = EBuiltInOps::CopyBufferToImage3d;
    MultiDispatchInfo dispatchInfo(dc);

    dispatchBcsOrGpgpuEnqueue<CL_COMMAND_WRITE_IMAGE>(dispatchInfo, surfaces, eBuiltInOps, numEventsInWaitList, eventWaitList, event, blockingWrite == CL_TRUE, csr);

    if (context->isProvidingPerformanceHints()) {
        context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_NEUTRAL_INTEL, CL_ENQUEUE_WRITE_IMAGE_REQUIRES_COPY_DATA, static_cast<cl_mem>(dstImage));
    }

    return CL_SUCCESS;
}
} // namespace NEO
