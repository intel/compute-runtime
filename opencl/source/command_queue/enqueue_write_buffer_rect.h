/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_stream/command_stream_receiver.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/memory_manager/mem_obj_surface.h"

#include <new>

namespace NEO {

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

    const cl_command_type cmdType = CL_COMMAND_WRITE_BUFFER_RECT;
    auto isMemTransferNeeded = true;
    if (buffer->isMemObjZeroCopy()) {
        size_t bufferOffset;
        size_t hostOffset;
        computeOffsetsValueForRectCommands(&bufferOffset, &hostOffset, bufferOrigin, hostOrigin, region, bufferRowPitch, bufferSlicePitch, hostRowPitch, hostSlicePitch);
        isMemTransferNeeded = buffer->checkIfMemoryTransferIsRequired(bufferOffset, hostOffset, ptr, cmdType);
    }
    if (!isMemTransferNeeded) {
        return enqueueMarkerForReadWriteOperation(buffer, const_cast<void *>(ptr), cmdType, blockingWrite,
                                                  numEventsInWaitList, eventWaitList, event);
    }

    auto eBuiltInOps = EBuiltInOps::CopyBufferRect;
    if (forceStateless(buffer->getSize())) {
        eBuiltInOps = EBuiltInOps::CopyBufferRectStateless;
    }
    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(eBuiltInOps,
                                                                            this->getDevice());
    BuiltInOwnershipWrapper builtInLock(builder, this->context);

    size_t hostPtrSize = Buffer::calculateHostPtrSize(hostOrigin, region, hostRowPitch, hostSlicePitch);
    void *srcPtr = const_cast<void *>(ptr);

    MemObjSurface dstBufferSurf(buffer);
    HostPtrSurface hostPtrSurf(srcPtr, hostPtrSize, true);
    Surface *surfaces[] = {&dstBufferSurf, &hostPtrSurf};

    if (region[0] != 0 &&
        region[1] != 0 &&
        region[2] != 0) {
        auto &csr = blitEnqueueAllowed(cmdType) ? *getBcsCommandStreamReceiver() : getGpgpuCommandStreamReceiver();
        bool status = csr.createAllocationForHostSurface(hostPtrSurf, false);
        if (!status) {
            return CL_OUT_OF_RESOURCES;
        }
        srcPtr = reinterpret_cast<void *>(hostPtrSurf.getAllocation()->getGpuAddress());
    }

    void *alignedSrcPtr = alignDown(srcPtr, 4);
    size_t srcPtrOffset = ptrDiff(srcPtr, alignedSrcPtr);

    BuiltinOpParams dc;
    dc.srcPtr = alignedSrcPtr;
    dc.dstMemObj = buffer;
    dc.srcOffset = hostOrigin;
    dc.srcOffset.x += srcPtrOffset;
    dc.dstOffset = bufferOrigin;
    dc.transferAllocation = hostPtrSurf.getAllocation();
    dc.size = region;
    dc.srcRowPitch = hostRowPitch;
    dc.srcSlicePitch = hostSlicePitch;
    dc.dstRowPitch = bufferRowPitch;
    dc.dstSlicePitch = bufferSlicePitch;

    MultiDispatchInfo dispatchInfo;
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
} // namespace NEO
