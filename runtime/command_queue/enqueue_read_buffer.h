/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/built_ins/built_ins.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_queue/enqueue_common.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/cache_policy.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/memory_manager/surface.h"

#include "hw_cmds.h"

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

    if (nullptr == mapAllocation) {
        notifyEnqueueReadBuffer(buffer, !!blockingRead);
    }

    bool isMemTransferNeeded = buffer->isMemObjZeroCopy() ? buffer->checkIfMemoryTransferIsRequired(offset, 0, ptr, CL_COMMAND_READ_BUFFER) : true;
    bool isCpuCopyAllowed = bufferCpuCopyAllowed(buffer, CL_COMMAND_READ_BUFFER, blockingRead, size, ptr,
                                                 numEventsInWaitList, eventWaitList);
    bool blitOperationsSupported = device->getExecutionEnvironment()->getHardwareInfo()->capabilityTable.blitterOperationsSupported &&
                                   DebugManager.flags.EnableBlitterOperationsForReadWriteBuffers.get();

    if (isCpuCopyAllowed) {
        if (isMemTransferNeeded) {
            return enqueueReadWriteBufferOnCpuWithMemoryTransfer(CL_COMMAND_READ_BUFFER, buffer, offset, size, ptr,
                                                                 numEventsInWaitList, eventWaitList, event);
        } else {
            return enqueueReadWriteBufferOnCpuWithoutMemoryTransfer(CL_COMMAND_READ_BUFFER, buffer, offset, size, ptr,
                                                                    numEventsInWaitList, eventWaitList, event);
        }
    } else if (!isMemTransferNeeded) {
        return enqueueMarkerForReadWriteOperation(buffer, ptr, CL_COMMAND_READ_BUFFER, blockingRead,
                                                  numEventsInWaitList, eventWaitList, event);
    } else if (blitOperationsSupported) {
        return enqueueReadWriteBufferWithBlitTransfer(CL_COMMAND_READ_BUFFER, buffer, offset, size, ptr,
                                                      numEventsInWaitList, eventWaitList, event);
    }

    auto &builder = getDevice().getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer,
                                                                                                        this->getContext(), this->getDevice());
    BuiltInOwnershipWrapper builtInLock(builder, this->context);
    MultiDispatchInfo dispatchInfo;

    void *dstPtr = ptr;

    MemObjSurface bufferSurf(buffer);
    HostPtrSurface hostPtrSurf(dstPtr, size);
    GeneralSurface mapSurface;
    Surface *surfaces[] = {&bufferSurf, nullptr};

    if (mapAllocation) {
        surfaces[1] = &mapSurface;
        mapSurface.setGraphicsAllocation(mapAllocation);
        //get offset between base cpu ptr of map allocation and dst ptr
        size_t dstOffset = ptrDiff(dstPtr, mapAllocation->getUnderlyingBuffer());
        dstPtr = reinterpret_cast<void *>(mapAllocation->getGpuAddress() + dstOffset);
    } else {
        surfaces[1] = &hostPtrSurf;
        if (size != 0) {
            bool status = getCommandStreamReceiver().createAllocationForHostSurface(hostPtrSurf, true);
            if (!status) {
                return CL_OUT_OF_RESOURCES;
            }
            dstPtr = reinterpret_cast<void *>(hostPtrSurf.getAllocation()->getGpuAddress());
        }
    }
    void *alignedDstPtr = alignDown(dstPtr, 4);
    size_t dstPtrOffset = ptrDiff(dstPtr, alignedDstPtr);

    BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
    dc.dstPtr = alignedDstPtr;
    dc.dstOffset = {dstPtrOffset, 0, 0};
    dc.srcMemObj = buffer;
    dc.srcOffset = {offset, 0, 0};
    dc.size = {size, 0, 0};
    builder.buildDispatchInfos(dispatchInfo, dc);

    if (context->isProvidingPerformanceHints()) {
        context->providePerformanceHintForMemoryTransfer(CL_COMMAND_READ_BUFFER, true, static_cast<cl_mem>(buffer), ptr);
        if (!isL3Capable(ptr, size)) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, CL_ENQUEUE_READ_BUFFER_DOESNT_MEET_ALIGNMENT_RESTRICTIONS, ptr, size, MemoryConstants::pageSize, MemoryConstants::pageSize);
        }
    }
    enqueueHandler<CL_COMMAND_READ_BUFFER>(
        surfaces,
        blockingRead == CL_TRUE,
        dispatchInfo,
        numEventsInWaitList,
        eventWaitList,
        event);

    return CL_SUCCESS;
}
} // namespace NEO
