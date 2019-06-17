/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/built_ins/built_ins.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/hardware_commands_helper.h"
#include "runtime/helpers/string.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/memory_manager/surface.h"

#include "hw_cmds.h"

#include <new>

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

    auto isMemTransferNeeded = buffer->isMemObjZeroCopy() ? buffer->checkIfMemoryTransferIsRequired(offset, 0, ptr, CL_COMMAND_WRITE_BUFFER) : true;
    bool isCpuCopyAllowed = bufferCpuCopyAllowed(buffer, CL_COMMAND_WRITE_BUFFER, blockingWrite, size, const_cast<void *>(ptr),
                                                 numEventsInWaitList, eventWaitList);
    bool blitOperationsSupported = device->getExecutionEnvironment()->getHardwareInfo()->capabilityTable.blitterOperationsSupported &&
                                   DebugManager.flags.EnableBlitterOperationsForReadWriteBuffers.get();

    if (isCpuCopyAllowed) {
        if (isMemTransferNeeded) {
            return enqueueReadWriteBufferOnCpuWithMemoryTransfer(CL_COMMAND_WRITE_BUFFER, buffer, offset, size, const_cast<void *>(ptr),
                                                                 numEventsInWaitList, eventWaitList, event);
        } else {
            return enqueueReadWriteBufferOnCpuWithoutMemoryTransfer(CL_COMMAND_WRITE_BUFFER, buffer, offset, size, const_cast<void *>(ptr),
                                                                    numEventsInWaitList, eventWaitList, event);
        }
    } else if (!isMemTransferNeeded) {
        return enqueueMarkerForReadWriteOperation(buffer, const_cast<void *>(ptr), CL_COMMAND_WRITE_BUFFER, blockingWrite,
                                                  numEventsInWaitList, eventWaitList, event);
    } else if (blitOperationsSupported) {
        return enqueueReadWriteBufferWithBlitTransfer(CL_COMMAND_WRITE_BUFFER, buffer, !!blockingWrite, offset, size, const_cast<void *>(ptr),
                                                      numEventsInWaitList, eventWaitList, event);
    }

    auto &builder = getDevice().getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer,
                                                                                                        this->getContext(), this->getDevice());

    BuiltInOwnershipWrapper builtInLock(builder, this->context);
    MultiDispatchInfo dispatchInfo;

    void *srcPtr = const_cast<void *>(ptr);

    HostPtrSurface hostPtrSurf(srcPtr, size, true);
    MemObjSurface bufferSurf(buffer);
    GeneralSurface mapSurface;
    Surface *surfaces[] = {&bufferSurf, nullptr};

    if (mapAllocation) {
        surfaces[1] = &mapSurface;
        mapSurface.setGraphicsAllocation(mapAllocation);
        //get offset between base cpu ptr of map allocation and dst ptr
        size_t srcOffset = ptrDiff(srcPtr, mapAllocation->getUnderlyingBuffer());
        srcPtr = reinterpret_cast<void *>(mapAllocation->getGpuAddress() + srcOffset);
    } else {
        surfaces[1] = &hostPtrSurf;
        if (size != 0) {
            bool status = getCommandStreamReceiver().createAllocationForHostSurface(hostPtrSurf, false);
            if (!status) {
                return CL_OUT_OF_RESOURCES;
            }
            srcPtr = reinterpret_cast<void *>(hostPtrSurf.getAllocation()->getGpuAddress());
        }
    }
    void *alignedSrcPtr = alignDown(srcPtr, 4);
    size_t srcPtrOffset = ptrDiff(srcPtr, alignedSrcPtr);

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
} // namespace NEO
