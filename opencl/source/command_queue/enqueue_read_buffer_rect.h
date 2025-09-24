/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_stream/command_stream_receiver.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/memory_manager/mem_obj_surface.h"

namespace NEO {

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueReadBufferRect(
    Buffer *buffer,
    cl_bool blockingRead,
    const size_t *bufferOrigin,
    const size_t *hostOrigin,
    const size_t *region,
    size_t bufferRowPitch,
    size_t bufferSlicePitch,
    size_t hostRowPitch,
    size_t hostSlicePitch,
    void *ptr,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {
    const cl_command_type cmdType = CL_COMMAND_READ_BUFFER_RECT;

    CsrSelectionArgs csrSelectionArgs{cmdType, buffer, {}, device->getRootDeviceIndex(), region};
    CommandStreamReceiver &csr = selectCsrForBuiltinOperation(csrSelectionArgs);

    auto isMemTransferNeeded = true;
    if (buffer->isMemObjZeroCopy()) {
        size_t bufferOffset;
        size_t hostOffset;
        computeOffsetsValueForRectCommands(&bufferOffset, &hostOffset, bufferOrigin, hostOrigin, region, bufferRowPitch, bufferSlicePitch, hostRowPitch, hostSlicePitch);
        isMemTransferNeeded = buffer->checkIfMemoryTransferIsRequired(bufferOffset, hostOffset, ptr, cmdType);
    }
    if (!isMemTransferNeeded) {
        return enqueueMarkerForReadWriteOperation(buffer, ptr, cmdType, blockingRead,
                                                  numEventsInWaitList, eventWaitList, event);
    }

    const size_t hostPtrSize = Buffer::calculateHostPtrSize(hostOrigin, region, hostRowPitch, hostSlicePitch);
    const uint32_t rootDeviceIndex = getDevice().getRootDeviceIndex();
    InternalMemoryType memoryType = InternalMemoryType::notSpecified;
    GraphicsAllocation *mapAllocation = nullptr;
    bool isCpuCopyAllowed = false;
    getContext().tryGetExistingHostPtrAllocation(ptr, hostPtrSize, rootDeviceIndex, mapAllocation, memoryType, isCpuCopyAllowed);

    const bool isStateless = forceStateless(buffer->getSize());
    const bool useHeapless = this->getHeaplessModeEnabled();
    auto builtInType = EBuiltInOps::adjustBuiltinType<EBuiltInOps::copyBufferRect>(isStateless, useHeapless);

    void *dstPtr = ptr;

    MemObjSurface srcBufferSurf(buffer);
    HostPtrSurface hostPtrSurf(dstPtr, hostPtrSize);
    GeneralSurface mapSurface;
    Surface *surfaces[] = {&srcBufferSurf, nullptr};

    auto bcsSplit = this->isSplitEnqueueBlitNeeded(csrSelectionArgs.direction, getTotalSizeFromRectRegion(region), csr);

    if (region[0] != 0 && region[1] != 0 && region[2] != 0) {
        if (mapAllocation) {
            surfaces[1] = &mapSurface;
            mapSurface.setGraphicsAllocation(mapAllocation);
            dstPtr = convertAddressWithOffsetToGpuVa(dstPtr, memoryType, *mapAllocation);
        } else {
            surfaces[1] = &hostPtrSurf;
            bool status = selectCsrForHostPtrAllocation(bcsSplit, csr).createAllocationForHostSurface(hostPtrSurf, true);
            if (!status) {
                return CL_OUT_OF_RESOURCES;
            }
            this->prepareHostPtrSurfaceForSplit(bcsSplit, *hostPtrSurf.getAllocation());
            dstPtr = reinterpret_cast<void *>(hostPtrSurf.getAllocation()->getGpuAddress());
        }
    }

    void *alignedDstPtr = alignDown(dstPtr, 4);
    size_t dstPtrOffset = ptrDiff(dstPtr, alignedDstPtr);

    BuiltinOpParams dc;
    dc.srcMemObj = buffer;
    dc.dstPtr = alignedDstPtr;
    dc.srcOffset = bufferOrigin;
    dc.dstOffset = hostOrigin;
    dc.transferAllocation = hostPtrSurf.getAllocation();
    dc.dstOffset.x += dstPtrOffset;
    dc.size = region;
    dc.srcRowPitch = bufferRowPitch;
    dc.srcSlicePitch = bufferSlicePitch;
    dc.dstRowPitch = hostRowPitch;
    dc.dstSlicePitch = hostSlicePitch;
    dc.bcsSplit = bcsSplit;
    dc.direction = csrSelectionArgs.direction;

    MultiDispatchInfo dispatchInfo(dc);
    const auto dispatchResult = dispatchBcsOrGpgpuEnqueue<CL_COMMAND_READ_BUFFER_RECT>(dispatchInfo, surfaces, builtInType, numEventsInWaitList, eventWaitList, event, blockingRead, csr);
    if (dispatchResult != CL_SUCCESS) {
        return dispatchResult;
    }

    if (context->isProvidingPerformanceHints()) {
        context->providePerformanceHintForMemoryTransfer(CL_COMMAND_READ_BUFFER_RECT, true, static_cast<cl_mem>(buffer), ptr);
        if (!isL3Capable(ptr, hostPtrSize)) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, CL_ENQUEUE_READ_BUFFER_RECT_DOESNT_MEET_ALIGNMENT_RESTRICTIONS, ptr, hostPtrSize, MemoryConstants::pageSize, MemoryConstants::pageSize);
        }
    }

    return CL_SUCCESS;
}
} // namespace NEO
