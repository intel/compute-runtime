/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/memory_manager/mem_obj_surface.h"

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

    const cl_command_type cmdType = CL_COMMAND_WRITE_BUFFER;
    auto isMemTransferNeeded = buffer->isMemObjZeroCopy() ? buffer->checkIfMemoryTransferIsRequired(offset, 0, ptr, cmdType) : true;
    bool isCpuCopyAllowed = bufferCpuCopyAllowed(buffer, cmdType, blockingWrite, size, const_cast<void *>(ptr),
                                                 numEventsInWaitList, eventWaitList);

    //check if we are dealing with SVM pointer here for which we already have an allocation
    if (!mapAllocation && this->getContext().getSVMAllocsManager()) {
        auto svmEntry = this->getContext().getSVMAllocsManager()->getSVMAlloc(ptr);
        if (svmEntry) {
            if ((svmEntry->gpuAllocation->getGpuAddress() + svmEntry->size) < (castToUint64(ptr) + size)) {
                return CL_INVALID_OPERATION;
            }

            if (isCpuCopyAllowed) {
                if (svmEntry->memoryType == DEVICE_UNIFIED_MEMORY) {
                    isCpuCopyAllowed = false;
                }
            }

            mapAllocation = svmEntry->cpuAllocation ? svmEntry->cpuAllocation : svmEntry->gpuAllocation;
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

    auto eBuiltInOps = EBuiltInOps::CopyBufferToBuffer;
    if (forceStateless(buffer->getSize())) {
        eBuiltInOps = EBuiltInOps::CopyBufferToBufferStateless;
    }
    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(eBuiltInOps,
                                                                            this->getDevice());

    BuiltInOwnershipWrapper builtInLock(builder, this->context);

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
            auto &csr = blitEnqueueAllowed(cmdType) ? *getBcsCommandStreamReceiver() : getGpgpuCommandStreamReceiver();
            bool status = csr.createAllocationForHostSurface(hostPtrSurf, false);
            if (!status) {
                return CL_OUT_OF_RESOURCES;
            }
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

    MultiDispatchInfo dispatchInfo;
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
