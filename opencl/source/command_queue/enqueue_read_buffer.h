/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/os_context.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/command_queue/enqueue_common.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/memory_manager/mem_obj_surface.h"

#include <new>

namespace NEO {

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueReadBuffer(
    Buffer *buffer,
    cl_bool blockingRead,
    size_t offset,
    size_t size,
    void *ptr,
    GraphicsAllocation *mapAllocation,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {
    const cl_command_type cmdType = CL_COMMAND_READ_BUFFER;

    CsrSelectionArgs csrSelectionArgs{cmdType, buffer, {}, device->getRootDeviceIndex(), &size};
    CommandStreamReceiver &csr = selectCsrForBuiltinOperation(csrSelectionArgs);
    return enqueueReadBufferImpl(buffer, blockingRead, offset, size, ptr, mapAllocation, numEventsInWaitList, eventWaitList, event, csr);
}

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueReadBufferImpl(
    Buffer *buffer,
    cl_bool blockingRead,
    size_t offset,
    size_t size,
    void *ptr,
    GraphicsAllocation *mapAllocation,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event, CommandStreamReceiver &csr) {

    const cl_command_type cmdType = CL_COMMAND_READ_BUFFER;

    CsrSelectionArgs csrSelectionArgs{cmdType, buffer, {}, device->getRootDeviceIndex(), &size};

    if (nullptr == mapAllocation) {
        notifyEnqueueReadBuffer(buffer, !!blockingRead, EngineHelpers::isBcs(csr.getOsContext().getEngineType()));
    }

    auto rootDeviceIndex = getDevice().getRootDeviceIndex();
    bool isMemTransferNeeded = buffer->isMemObjZeroCopy() ? buffer->checkIfMemoryTransferIsRequired(offset, 0, ptr, cmdType) : true;
    bool isCpuCopyAllowed = bufferCpuCopyAllowed(buffer, cmdType, blockingRead, size, ptr,
                                                 numEventsInWaitList, eventWaitList);
    InternalMemoryType memoryType = InternalMemoryType::notSpecified;

    if (!mapAllocation) {
        cl_int retVal = getContext().tryGetExistingHostPtrAllocation(ptr, size, rootDeviceIndex, mapAllocation, memoryType, isCpuCopyAllowed);
        if (retVal != CL_SUCCESS) {
            return retVal;
        }
    }

    if (isCpuCopyAllowed) {
        if (isMemTransferNeeded) {
            return enqueueReadWriteBufferOnCpuWithMemoryTransfer(cmdType, buffer, offset, size, ptr,
                                                                 numEventsInWaitList, eventWaitList, event);
        } else {
            return enqueueReadWriteBufferOnCpuWithoutMemoryTransfer(cmdType, buffer, offset, size, ptr,
                                                                    numEventsInWaitList, eventWaitList, event);
        }
    } else if (!isMemTransferNeeded) {
        return enqueueMarkerForReadWriteOperation(buffer, ptr, cmdType, blockingRead,
                                                  numEventsInWaitList, eventWaitList, event);
    }

    const bool isStateless = forceStateless(buffer->getSize());
    const bool useHeapless = this->getHeaplessModeEnabled();
    auto builtInType = EBuiltInOps::adjustBuiltinType<EBuiltInOps::copyBufferToBuffer>(isStateless, useHeapless);

    void *dstPtr = ptr;

    MemObjSurface bufferSurf(buffer);
    HostPtrSurface hostPtrSurf(dstPtr, size);
    GeneralSurface mapSurface;
    Surface *surfaces[] = {&bufferSurf, nullptr};

    auto bcsSplit = this->isSplitEnqueueBlitNeeded(csrSelectionArgs.direction, size, csr);

    if (mapAllocation) {
        surfaces[1] = &mapSurface;
        mapSurface.setGraphicsAllocation(mapAllocation);
        dstPtr = convertAddressWithOffsetToGpuVa(dstPtr, memoryType, *mapAllocation);
    } else {
        surfaces[1] = &hostPtrSurf;
        if (size != 0) {
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
    dc.dstPtr = alignedDstPtr;
    dc.dstOffset = {dstPtrOffset, 0, 0};
    dc.srcMemObj = buffer;
    dc.srcOffset = {offset, 0, 0};
    dc.size = {size, 0, 0};
    dc.transferAllocation = mapAllocation ? mapAllocation : hostPtrSurf.getAllocation();
    dc.bcsSplit = bcsSplit;
    dc.direction = csrSelectionArgs.direction;

    MultiDispatchInfo dispatchInfo(dc);

    if (context->isProvidingPerformanceHints()) {
        context->providePerformanceHintForMemoryTransfer(CL_COMMAND_READ_BUFFER, true, static_cast<cl_mem>(buffer), ptr);
        if (!isL3Capable(ptr, size)) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, CL_ENQUEUE_READ_BUFFER_DOESNT_MEET_ALIGNMENT_RESTRICTIONS, ptr, size, MemoryConstants::pageSize, MemoryConstants::pageSize);
        }
    }

    return dispatchBcsOrGpgpuEnqueue<CL_COMMAND_READ_BUFFER>(dispatchInfo, surfaces, builtInType, numEventsInWaitList, eventWaitList, event, blockingRead, csr);
}

} // namespace NEO
