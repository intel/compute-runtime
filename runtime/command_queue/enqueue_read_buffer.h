/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "hw_cmds.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_queue/enqueue_common.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/cache_policy.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/memory_manager/surface.h"
#include "runtime/built_ins/built_ins.h"
#include <new>

namespace OCLRT {

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueReadBuffer(
    Buffer *buffer,
    cl_bool blockingRead,
    size_t offset,
    size_t size,
    void *ptr,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {

    notifyEnqueueReadBuffer(buffer, !!blockingRead);

    cl_int retVal = CL_SUCCESS;
    bool isMemTransferNeeded = buffer->isMemObjZeroCopy() ? buffer->checkIfMemoryTransferIsRequired(offset, 0, ptr, CL_COMMAND_READ_BUFFER) : true;
    if ((DebugManager.flags.DoCpuCopyOnReadBuffer.get() ||
         buffer->isReadWriteOnCpuAllowed(blockingRead, numEventsInWaitList, ptr, size)) &&
        context->getDevice(0)->getDeviceInfo().cpuCopyAllowed) {
        if (!isMemTransferNeeded) {
            TransferProperties transferProperties(buffer, CL_COMMAND_MARKER, 0, true, &offset, &size, ptr);
            EventsRequest eventsRequest(numEventsInWaitList, eventWaitList, event);
            cpuDataTransferHandler(transferProperties, eventsRequest, retVal);
            if (event) {
                auto pEvent = castToObjectOrAbort<Event>(*event);
                pEvent->setCmdType(CL_COMMAND_READ_BUFFER);
            }

            if (context->isProvidingPerformanceHints()) {
                context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, CL_ENQUEUE_READ_BUFFER_DOESNT_REQUIRE_COPY_DATA, static_cast<cl_mem>(buffer), ptr);
            }
            return retVal;
        }
        TransferProperties transferProperties(buffer, CL_COMMAND_READ_BUFFER, 0, true, &offset, &size, ptr);
        EventsRequest eventsRequest(numEventsInWaitList, eventWaitList, event);
        cpuDataTransferHandler(transferProperties, eventsRequest, retVal);

        return retVal;
    }
    MultiDispatchInfo dispatchInfo;
    if (!isMemTransferNeeded) {
        NullSurface s;
        Surface *surfaces[] = {&s};
        enqueueHandler<CL_COMMAND_MARKER>(
            surfaces,
            blockingRead == CL_TRUE,
            dispatchInfo,
            numEventsInWaitList,
            eventWaitList,
            event);
        if (event) {
            auto pEvent = castToObjectOrAbort<Event>(*event);
            pEvent->setCmdType(CL_COMMAND_READ_BUFFER);
        }

        if (context->isProvidingPerformanceHints()) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, CL_ENQUEUE_READ_BUFFER_DOESNT_REQUIRE_COPY_DATA, static_cast<cl_mem>(buffer), ptr);
        }

        return CL_SUCCESS;
    }
    auto &builder = getDevice().getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer,
                                                                                                        this->getContext(), this->getDevice());
    BuiltInOwnershipWrapper builtInLock(builder, this->context);

    void *dstPtr = ptr;
    void *alignedDstPtr = dstPtr;
    size_t dstPtrOffset = 0;

    if (!isAligned<4>(dstPtr)) {
        alignedDstPtr = alignDown(dstPtr, 4);
        dstPtrOffset = ptrDiff(dstPtr, alignedDstPtr);
    }

    MemObjSurface bufferSurf(buffer);
    HostPtrSurface hostPtrSurf(alignedDstPtr, size + dstPtrOffset);
    Surface *surfaces[] = {&bufferSurf, &hostPtrSurf};

    if (size != 0) {
        bool status = getDevice().getCommandStreamReceiver().createAllocationForHostSurface(hostPtrSurf, getDevice(), true);
        if (!status) {
            return CL_OUT_OF_RESOURCES;
        }

        hostPtrSurf.getAllocation()->allocationOffset = dstPtrOffset;
    }

    BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
    dc.dstPtr = alignedDstPtr;
    dc.dstOffset = {dstPtrOffset, 0, 0};
    dc.srcMemObj = buffer;
    dc.srcOffset = {offset, 0, 0};
    dc.size = {size, 0, 0};
    builder.buildDispatchInfos(dispatchInfo, dc);

    if (context->isProvidingPerformanceHints()) {
        context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, CL_ENQUEUE_READ_BUFFER_REQUIRES_COPY_DATA, static_cast<cl_mem>(buffer), ptr);
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
} // namespace OCLRT
