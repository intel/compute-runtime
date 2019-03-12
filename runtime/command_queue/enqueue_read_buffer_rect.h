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
#include "runtime/helpers/kernel_commands.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/memory_manager/surface.h"

#include <new>

namespace OCLRT {

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

    MultiDispatchInfo dispatchInfo;
    auto isMemTransferNeeded = true;
    if (buffer->isMemObjZeroCopy()) {
        size_t bufferOffset;
        size_t hostOffset;
        computeOffsetsValueForRectCommands(&bufferOffset, &hostOffset, bufferOrigin, hostOrigin, region, bufferRowPitch, bufferSlicePitch, hostRowPitch, hostSlicePitch);
        isMemTransferNeeded = buffer->checkIfMemoryTransferIsRequired(bufferOffset, hostOffset, ptr, CL_COMMAND_READ_BUFFER_RECT);
    }
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
            pEvent->setCmdType(CL_COMMAND_READ_BUFFER_RECT);
        }

        if (context->isProvidingPerformanceHints()) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, CL_ENQUEUE_READ_BUFFER_RECT_DOESNT_REQUIRES_COPY_DATA, static_cast<cl_mem>(buffer), ptr);
        }

        return CL_SUCCESS;
    }
    auto &builder = getDevice().getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferRect,
                                                                                                        this->getContext(), this->getDevice());
    BuiltInOwnershipWrapper builtInLock(builder, this->context);

    size_t hostPtrSize = Buffer::calculateHostPtrSize(hostOrigin, region, hostRowPitch, hostSlicePitch);
    void *dstPtr = ptr;

    MemObjSurface bufferSurf(buffer);
    HostPtrSurface hostPtrSurf(dstPtr, hostPtrSize);
    Surface *surfaces[] = {&bufferSurf, &hostPtrSurf};

    if (region[0] != 0 &&
        region[1] != 0 &&
        region[2] != 0) {
        bool status = getCommandStreamReceiver().createAllocationForHostSurface(hostPtrSurf, true);
        if (!status) {
            return CL_OUT_OF_RESOURCES;
        }
        dstPtr = reinterpret_cast<void *>(hostPtrSurf.getAllocation()->getGpuAddress());
    }

    void *alignedDstPtr = alignDown(dstPtr, 4);
    size_t dstPtrOffset = ptrDiff(dstPtr, alignedDstPtr);

    BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
    dc.srcMemObj = buffer;
    dc.dstPtr = alignedDstPtr;
    dc.srcOffset = bufferOrigin;
    dc.dstOffset = hostOrigin;
    dc.dstOffset.x += dstPtrOffset;
    dc.size = region;
    dc.srcRowPitch = bufferRowPitch;
    dc.srcSlicePitch = bufferSlicePitch;
    dc.dstRowPitch = hostRowPitch;
    dc.dstSlicePitch = hostSlicePitch;
    builder.buildDispatchInfos(dispatchInfo, dc);

    enqueueHandler<CL_COMMAND_READ_BUFFER_RECT>(
        surfaces,
        blockingRead == CL_TRUE,
        dispatchInfo,
        numEventsInWaitList,
        eventWaitList,
        event);

    if (context->isProvidingPerformanceHints()) {
        context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, CL_ENQUEUE_READ_BUFFER_RECT_REQUIRES_COPY_DATA, static_cast<cl_mem>(buffer), ptr);
        if (!isL3Capable(ptr, hostPtrSize)) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, CL_ENQUEUE_READ_BUFFER_RECT_DOESNT_MEET_ALIGNMENT_RESTRICTIONS, ptr, hostPtrSize, MemoryConstants::pageSize, MemoryConstants::pageSize);
        }
    }

    return CL_SUCCESS;
}
} // namespace OCLRT
