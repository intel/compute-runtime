/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/memory_manager/mem_obj_surface.h"

namespace NEO {

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueWriteBuffer(
    Buffer *buffer,
    cl_bool blockingWrite,
    size_t offset,
    size_t size,
    const void *ptr,
    GraphicsAllocation *mapAllocation,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {
    const cl_command_type cmdType = CL_COMMAND_WRITE_BUFFER;

    CsrSelectionArgs csrSelectionArgs{cmdType, {}, buffer, device->getRootDeviceIndex(), &size};
    CommandStreamReceiver &csr = selectCsrForBuiltinOperation(csrSelectionArgs);
    return enqueueWriteBufferImpl(buffer, blockingWrite, offset, size, ptr, mapAllocation, numEventsInWaitList, eventWaitList, event, csr);
}

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueWriteBufferImpl(
    Buffer *buffer,
    cl_bool blockingWrite,
    size_t offset,
    size_t size,
    const void *ptr,
    GraphicsAllocation *mapAllocation,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event,
    CommandStreamReceiver &csr) {
    const cl_command_type cmdType = CL_COMMAND_WRITE_BUFFER;

    CsrSelectionArgs csrSelectionArgs{cmdType, {}, buffer, device->getRootDeviceIndex(), &size};

    auto rootDeviceIndex = getDevice().getRootDeviceIndex();
    auto isMemTransferNeeded = buffer->isMemObjZeroCopy() ? buffer->checkIfMemoryTransferIsRequired(offset, 0, ptr, cmdType) : true;
    bool isCpuCopyAllowed = bufferCpuCopyAllowed(buffer, cmdType, blockingWrite, size, const_cast<void *>(ptr),
                                                 numEventsInWaitList, eventWaitList);
    InternalMemoryType memoryType = InternalMemoryType::notSpecified;

    if (!mapAllocation) {
        cl_int retVal = getContext().tryGetExistingHostPtrAllocation(ptr, size, rootDeviceIndex, mapAllocation, memoryType, isCpuCopyAllowed);
        if (retVal != CL_SUCCESS) {
            return retVal;
        }
        if (mapAllocation) {
            mapAllocation->setAubWritable(true, GraphicsAllocation::defaultBank);
            mapAllocation->setTbxWritable(true, GraphicsAllocation::defaultBank);
        }
    }

    if (isCpuCopyAllowed) {
        if (isMemTransferNeeded) {
            return enqueueReadWriteBufferOnCpuWithMemoryTransfer(cmdType, buffer, offset, size, const_cast<void *>(ptr),
                                                                 numEventsInWaitList, eventWaitList, event);
        } else {
            return enqueueReadWriteBufferOnCpuWithoutMemoryTransfer(cmdType, buffer, offset, size, const_cast<void *>(ptr),
                                                                    numEventsInWaitList, eventWaitList, event);
        }
    } else if (!isMemTransferNeeded) {
        return enqueueMarkerForReadWriteOperation(buffer, const_cast<void *>(ptr), cmdType, blockingWrite,
                                                  numEventsInWaitList, eventWaitList, event);
    }

    const bool isStateless = forceStateless(buffer->getSize());
    const bool useHeapless = this->getHeaplessModeEnabled();
    auto builtInType = EBuiltInOps::adjustBuiltinType<EBuiltInOps::copyBufferToBuffer>(isStateless, useHeapless);

    void *srcPtr = const_cast<void *>(ptr);

    HostPtrSurface hostPtrSurf(srcPtr, size, true);
    MemObjSurface bufferSurf(buffer);
    GeneralSurface mapSurface;
    Surface *surfaces[] = {&bufferSurf, nullptr};

    auto bcsSplit = this->isSplitEnqueueBlitNeeded(csrSelectionArgs.direction, size, csr);

    if (mapAllocation) {
        surfaces[1] = &mapSurface;
        mapSurface.setGraphicsAllocation(mapAllocation);
        srcPtr = convertAddressWithOffsetToGpuVa(srcPtr, memoryType, *mapAllocation);
    } else {
        surfaces[1] = &hostPtrSurf;
        if (size != 0) {
            bool status = selectCsrForHostPtrAllocation(bcsSplit, csr).createAllocationForHostSurface(hostPtrSurf, false);
            if (!status) {
                return CL_OUT_OF_RESOURCES;
            }
            this->prepareHostPtrSurfaceForSplit(bcsSplit, *hostPtrSurf.getAllocation());

            srcPtr = reinterpret_cast<void *>(hostPtrSurf.getAllocation()->getGpuAddress());
        }
    }
    void *alignedSrcPtr = alignDown(srcPtr, 4);
    size_t srcPtrOffset = ptrDiff(srcPtr, alignedSrcPtr);

    BuiltinOpParams dc;
    dc.srcPtr = alignedSrcPtr;
    dc.srcOffset = {srcPtrOffset, 0, 0};
    dc.dstMemObj = buffer;
    dc.dstOffset = {offset, 0, 0};
    dc.size = {size, 0, 0};
    dc.transferAllocation = mapAllocation ? mapAllocation : hostPtrSurf.getAllocation();
    dc.bcsSplit = bcsSplit;
    dc.direction = csrSelectionArgs.direction;

    MultiDispatchInfo dispatchInfo(dc);
    const auto dispatchResult = dispatchBcsOrGpgpuEnqueue<CL_COMMAND_WRITE_BUFFER>(dispatchInfo, surfaces, builtInType, numEventsInWaitList, eventWaitList, event, blockingWrite, csr);
    if (dispatchResult != CL_SUCCESS) {
        return dispatchResult;
    }

    if (context->isProvidingPerformanceHints()) {
        context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_NEUTRAL_INTEL, CL_ENQUEUE_WRITE_BUFFER_REQUIRES_COPY_DATA, static_cast<cl_mem>(buffer));
    }

    return CL_SUCCESS;
}
} // namespace NEO
