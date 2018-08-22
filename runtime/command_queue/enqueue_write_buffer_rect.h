/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/memory_manager/surface.h"
#include "runtime/built_ins/built_ins.h"
#include <new>

namespace OCLRT {

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueWriteBufferRect(
    Buffer *buffer,
    cl_bool blockingWrite,
    const size_t *bufferOrigin,
    const size_t *hostOrigin,
    const size_t *region,
    size_t bufferRowPitch,
    size_t bufferSlicePitch,
    size_t hostRowPitch,
    size_t hostSlicePitch,
    const void *ptr,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {

    MultiDispatchInfo dispatchInfo;
    auto isMemTransferNeeded = true;
    if (buffer->isMemObjZeroCopy()) {
        size_t bufferOffset;
        size_t hostOffset;
        computeOffsetsValueForRectCommands(&bufferOffset, &hostOffset, bufferOrigin, hostOrigin, region, bufferRowPitch, bufferSlicePitch, hostRowPitch, hostSlicePitch);
        isMemTransferNeeded = buffer->checkIfMemoryTransferIsRequired(bufferOffset, hostOffset, ptr, CL_COMMAND_WRITE_BUFFER_RECT);
    }
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
            pEvent->setCmdType(CL_COMMAND_WRITE_BUFFER_RECT);
        }

        if (context->isProvidingPerformanceHints()) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, CL_ENQUEUE_WRITE_BUFFER_RECT_DOESNT_REQUIRE_COPY_DATA, static_cast<cl_mem>(buffer), ptr);
        }

        return CL_SUCCESS;
    }
    auto &builder = getDevice().getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferRect,
                                                                                                        this->getContext(), this->getDevice());
    BuiltInOwnershipWrapper builtInLock(builder, this->context);

    size_t hostPtrSize = Buffer::calculateHostPtrSize(hostOrigin, region, hostRowPitch, hostSlicePitch);
    void *srcPtr = const_cast<void *>(ptr);

    MemObjSurface dstBufferSurf(buffer);
    HostPtrSurface hostPtrSurf(srcPtr, hostPtrSize, true);
    Surface *surfaces[] = {&dstBufferSurf, &hostPtrSurf};

    if (region[0] != 0 &&
        region[1] != 0 &&
        region[2] != 0) {
        bool status = createAllocationForHostSurface(hostPtrSurf);
        if (!status) {
            return CL_OUT_OF_RESOURCES;
        }
        srcPtr = reinterpret_cast<void *>(hostPtrSurf.getAllocation()->getGpuAddressToPatch());
    }

    BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
    dc.srcPtr = srcPtr;
    dc.dstMemObj = buffer;
    dc.srcOffset = hostOrigin;
    dc.dstOffset = bufferOrigin;
    dc.size = region;
    dc.srcRowPitch = hostRowPitch;
    dc.srcSlicePitch = hostSlicePitch;
    dc.dstRowPitch = bufferRowPitch;
    dc.dstSlicePitch = bufferSlicePitch;
    builder.buildDispatchInfos(dispatchInfo, dc);

    enqueueHandler<CL_COMMAND_WRITE_BUFFER_RECT>(
        surfaces,
        blockingWrite == CL_TRUE,
        dispatchInfo,
        numEventsInWaitList,
        eventWaitList,
        event);

    if (context->isProvidingPerformanceHints()) {
        context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_NEUTRAL_INTEL, CL_ENQUEUE_WRITE_BUFFER_RECT_REQUIRES_COPY_DATA, static_cast<cl_mem>(buffer));
    }

    return CL_SUCCESS;
}
} // namespace OCLRT
