/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "hw_cmds.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/helpers/string.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/memory_manager/surface.h"
#include "runtime/built_ins/built_ins.h"
#include <new>

namespace OCLRT {

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueWriteBuffer(
    Buffer *buffer,
    cl_bool blockingWrite,
    size_t offset,
    size_t size,
    const void *ptr,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {

    cl_int retVal = CL_SUCCESS;
    auto isMemTransferNeeded = buffer->isMemObjZeroCopy() ? buffer->checkIfMemoryTransferIsRequired(offset, 0, ptr, CL_COMMAND_READ_BUFFER) : true;
    if ((DebugManager.flags.DoCpuCopyOnWriteBuffer.get() ||
         buffer->isReadWriteOnCpuAllowed(blockingWrite, numEventsInWaitList, const_cast<void *>(ptr), size)) &&
        context->getDevice(0)->getDeviceInfo().cpuCopyAllowed) {
        if (!isMemTransferNeeded) {
            TransferProperties transferProperties(buffer, CL_COMMAND_MARKER, 0, true, &offset, &size, const_cast<void *>(ptr));
            EventsRequest eventsRequest(numEventsInWaitList, eventWaitList, event);
            cpuDataTransferHandler(transferProperties, eventsRequest, retVal);

            if (event) {
                auto pEvent = castToObjectOrAbort<Event>(*event);
                pEvent->setCmdType(CL_COMMAND_WRITE_BUFFER);
            }

            if (context->isProvidingPerformanceHints()) {
                context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, CL_ENQUEUE_WRITE_BUFFER_DOESNT_REQUIRE_COPY_DATA, static_cast<cl_mem>(buffer), ptr);
            }
            return retVal;
        }
        TransferProperties transferProperties(buffer, CL_COMMAND_WRITE_BUFFER, 0, true, &offset, &size, const_cast<void *>(ptr));
        EventsRequest eventsRequest(numEventsInWaitList, eventWaitList, event);
        cpuDataTransferHandler(transferProperties, eventsRequest, retVal);
        if (DebugManager.flags.ForceResourceLockOnTransferCalls.get()) {
            if (transferProperties.lockedPtr != nullptr) {
                buffer->getMemoryManager()->unlockResource(buffer->getGraphicsAllocation());
            }
        }

        return retVal;
    }

    MultiDispatchInfo dispatchInfo;
    if (!isMemTransferNeeded) {
        NullSurface s;
        Surface *surfaces[] = {&s};
        enqueueHandler<CL_COMMAND_MARKER>(
            surfaces,
            blockingWrite == CL_TRUE,
            dispatchInfo,
            numEventsInWaitList,
            eventWaitList,
            event);
        if (event) {
            auto pEvent = castToObjectOrAbort<Event>(*event);
            pEvent->setCmdType(CL_COMMAND_WRITE_BUFFER);
        }

        if (context->isProvidingPerformanceHints()) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, CL_ENQUEUE_WRITE_BUFFER_DOESNT_REQUIRE_COPY_DATA, static_cast<cl_mem>(buffer), ptr);
        }

        return CL_SUCCESS;
    }
    auto &builder = getDevice().getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer,
                                                                                                        this->getContext(), this->getDevice());

    BuiltInOwnershipWrapper builtInLock(builder, this->context);

    void *srcPtr = const_cast<void *>(ptr);
    void *alignedSrcPtr = srcPtr;
    size_t srcPtrOffset = 0;

    if (!isAligned<4>(srcPtr)) {
        alignedSrcPtr = alignDown(srcPtr, 4);
        srcPtrOffset = ptrDiff(srcPtr, alignedSrcPtr);
    }

    HostPtrSurface hostPtrSurf(alignedSrcPtr, size + srcPtrOffset, true);
    MemObjSurface bufferSurf(buffer);
    Surface *surfaces[] = {&bufferSurf, &hostPtrSurf};

    if (size != 0) {
        bool status = getCommandStreamReceiver().createAllocationForHostSurface(hostPtrSurf, getDevice(), false);
        if (!status) {
            return CL_OUT_OF_RESOURCES;
        }

        hostPtrSurf.getAllocation()->allocationOffset += srcPtrOffset;
    }

    BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
    dc.srcPtr = alignedSrcPtr;
    dc.srcOffset = {srcPtrOffset, 0, 0};
    dc.dstMemObj = buffer;
    dc.dstOffset = {offset, 0, 0};
    dc.size = {size, 0, 0};
    builder.buildDispatchInfos(dispatchInfo, dc);

    enqueueHandler<CL_COMMAND_WRITE_BUFFER>(
        surfaces,
        blockingWrite == CL_TRUE,
        dispatchInfo,
        numEventsInWaitList,
        eventWaitList,
        event);

    if (context->isProvidingPerformanceHints()) {
        context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_NEUTRAL_INTEL, CL_ENQUEUE_WRITE_BUFFER_REQUIRES_COPY_DATA, static_cast<cl_mem>(buffer));
    }

    return CL_SUCCESS;
}
} // namespace OCLRT
