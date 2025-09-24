/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/os_interface/os_context.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/mipmap.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/memory_manager/mem_obj_surface.h"

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
    constexpr cl_command_type cmdType = CL_COMMAND_READ_IMAGE;

    CsrSelectionArgs csrSelectionArgs{cmdType, srcImage, {}, device->getRootDeviceIndex(), region, origin, nullptr};
    CommandStreamReceiver &csr = selectCsrForBuiltinOperation(csrSelectionArgs);
    return enqueueReadImageImpl(srcImage, blockingRead, origin, region, inputRowPitch, inputSlicePitch, ptr, mapAllocation, numEventsInWaitList, eventWaitList, event, csr);
}

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueReadImageImpl(
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
    cl_event *event, CommandStreamReceiver &csr) {
    constexpr cl_command_type cmdType = CL_COMMAND_READ_IMAGE;

    CsrSelectionArgs csrSelectionArgs{cmdType, srcImage, {}, device->getRootDeviceIndex(), region, origin, nullptr};

    if (nullptr == mapAllocation) {
        notifyEnqueueReadImage(srcImage, static_cast<bool>(blockingRead), EngineHelpers::isBcs(csr.getOsContext().getEngineType()));
    }

    auto isMemTransferNeeded = true;
    if (srcImage->isMemObjZeroCopy()) {
        size_t hostOffset;
        Image::calculateHostPtrOffset(&hostOffset, origin, region, inputRowPitch, inputSlicePitch, srcImage->getImageDesc().image_type, srcImage->getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes);
        isMemTransferNeeded = srcImage->checkIfMemoryTransferIsRequired(hostOffset, 0, ptr, cmdType);
    }
    if (!isMemTransferNeeded) {
        return enqueueMarkerForReadWriteOperation(srcImage, ptr, cmdType, blockingRead,
                                                  numEventsInWaitList, eventWaitList, event);
    }

    size_t hostPtrSize = calculateHostPtrSizeForImage(region, inputRowPitch, inputSlicePitch, srcImage);
    void *dstPtr = ptr;

    MemObjSurface srcImgSurf(srcImage);
    HostPtrSurface hostPtrSurf(dstPtr, hostPtrSize);
    GeneralSurface mapSurface;
    Surface *surfaces[] = {&srcImgSurf, nullptr};

    auto bcsSplit = this->isSplitEnqueueBlitNeeded(csrSelectionArgs.direction, getTotalSizeFromRectRegion(region), csr);

    bool tempAllocFallback = false;

    if (!mapAllocation) {
        InternalMemoryType memoryType = InternalMemoryType::notSpecified;
        bool isCpuCopyAllowed = false;
        cl_int retVal = getContext().tryGetExistingHostPtrAllocation(ptr, hostPtrSize, device->getRootDeviceIndex(), mapAllocation, memoryType, isCpuCopyAllowed);
        if (retVal != CL_SUCCESS) {
            return retVal;
        }
    }

    if (mapAllocation) {
        surfaces[1] = &mapSurface;
        mapSurface.setGraphicsAllocation(mapAllocation);
        // get offset between base cpu ptr of map allocation and dst ptr
        size_t dstOffset = ptrDiff(dstPtr, mapAllocation->getUnderlyingBuffer());
        dstPtr = reinterpret_cast<void *>(mapAllocation->getGpuAddress() + dstOffset);
    } else {
        surfaces[1] = &hostPtrSurf;
        if (region[0] != 0 &&
            region[1] != 0 &&
            region[2] != 0) {
            bool status = selectCsrForHostPtrAllocation(bcsSplit, csr).createAllocationForHostSurface(hostPtrSurf, true);
            if (!status) {
                if (CL_TRUE == blockingRead) {
                    hostPtrSurf.setIsPtrCopyAllowed(true);
                    status = selectCsrForHostPtrAllocation(bcsSplit, csr).createAllocationForHostSurface(hostPtrSurf, true);
                    if (!status) {
                        return CL_OUT_OF_RESOURCES;
                    }
                    tempAllocFallback = true;
                } else {
                    return CL_OUT_OF_RESOURCES;
                }
            }
            dstPtr = reinterpret_cast<void *>(hostPtrSurf.getAllocation()->getGpuAddress());
            this->prepareHostPtrSurfaceForSplit(bcsSplit, *hostPtrSurf.getAllocation());
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
    dc.dstRowPitch = (srcImage->getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY) ? inputSlicePitch : inputRowPitch;
    dc.dstSlicePitch = inputSlicePitch;
    if (isMipMapped(srcImage->getImageDesc())) {
        dc.srcMipLevel = findMipLevel(srcImage->getImageDesc().image_type, origin);
    }
    dc.transferAllocation = mapAllocation ? mapAllocation : hostPtrSurf.getAllocation();
    if (tempAllocFallback) {
        dc.userPtrForPostOperationCpuCopy = ptr;
    }
    dc.bcsSplit = bcsSplit;
    dc.direction = csrSelectionArgs.direction;

    const bool isStateless = forceStateless(srcImage->getSize());
    const bool useHeapless = this->getHeaplessModeEnabled();
    auto eBuiltInOps = EBuiltInOps::adjustBuiltinType<EBuiltInOps::copyImage3dToBuffer>(isStateless, useHeapless);
    MultiDispatchInfo dispatchInfo(dc);

    const auto dispatchResult = dispatchBcsOrGpgpuEnqueue<CL_COMMAND_READ_IMAGE>(dispatchInfo, surfaces, eBuiltInOps, numEventsInWaitList, eventWaitList, event, blockingRead == CL_TRUE, csr);
    if (dispatchResult != CL_SUCCESS) {
        return dispatchResult;
    }

    if (context->isProvidingPerformanceHints()) {
        if (!isL3Capable(ptr, hostPtrSize)) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, CL_ENQUEUE_READ_IMAGE_DOESNT_MEET_ALIGNMENT_RESTRICTIONS, ptr, hostPtrSize, MemoryConstants::pageSize, MemoryConstants::pageSize);
        }
    }

    return CL_SUCCESS;
}
} // namespace NEO
