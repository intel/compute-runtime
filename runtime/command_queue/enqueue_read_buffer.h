/*
 * Copyright (c) 2017, Intel Corporation
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

    cl_int retVal = CL_SUCCESS;
    bool isMemTransferNeeded = buffer->isMemObjZeroCopy() ? buffer->checkIfMemoryTransferIsRequired(offset, 0, ptr, CL_COMMAND_READ_BUFFER) : true;
    if ((DebugManager.flags.DoCpuCopyOnReadBuffer.get() ||
         buffer->isReadWriteOnCpuAllowed(blockingRead, numEventsInWaitList, ptr, size)) &&
        context->getDevice(0)->getDeviceInfo().cpuCopyAllowed) {
        if (!isMemTransferNeeded) {
            cpuDataTransferHandler(buffer,
                                   CL_COMMAND_MARKER,
                                   CL_TRUE,
                                   offset,
                                   size,
                                   ptr,
                                   numEventsInWaitList,
                                   eventWaitList,
                                   event,
                                   retVal);
            if (event) {
                auto pEvent = castToObjectOrAbort<Event>(*event);
                pEvent->setCmdType(CL_COMMAND_READ_BUFFER);
            }

            if (context->isProvidingPerformanceHints()) {
                context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, CL_ENQUEUE_READ_BUFFER_DOESNT_REQUIRE_COPY_DATA, static_cast<cl_mem>(buffer), ptr);
            }
            return retVal;
        }
        cpuDataTransferHandler(buffer,
                               CL_COMMAND_READ_BUFFER,
                               CL_TRUE,
                               offset,
                               size,
                               ptr,
                               numEventsInWaitList,
                               eventWaitList,
                               event,
                               retVal);
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
    auto &builder = BuiltIns::getInstance().getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer,
                                                                          this->getContext(), this->getDevice());
    builder.takeOwnership(this->context);

    BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
    dc.dstPtr = ptr;
    dc.srcMemObj = buffer;
    dc.srcOffset = {offset, 0, 0};
    dc.size = {size, 0, 0};
    builder.buildDispatchInfos(dispatchInfo, dc);

    MemObjSurface s1(buffer);
    HostPtrSurface s2(ptr, size);
    Surface *surfaces[] = {&s1, &s2};

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
    builder.releaseOwnership();

    return CL_SUCCESS;
}
} // namespace OCLRT
