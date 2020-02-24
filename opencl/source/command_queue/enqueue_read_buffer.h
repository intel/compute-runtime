/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

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

    if (nullptr == mapAllocation) {
        notifyEnqueueReadBuffer(buffer, !!blockingRead);
    }

    const cl_command_type cmdType = CL_COMMAND_READ_BUFFER;
    bool isMemTransferNeeded = buffer->isMemObjZeroCopy() ? buffer->checkIfMemoryTransferIsRequired(offset, 0, ptr, cmdType) : true;
    bool isCpuCopyAllowed = bufferCpuCopyAllowed(buffer, cmdType, blockingRead, size, ptr,
                                                 numEventsInWaitList, eventWaitList);

    //check if we are dealing with SVM pointer here for which we already have an allocation
    if (!mapAllocation && this->getContext().getSVMAllocsManager()) {
        auto svmEntry = this->getContext().getSVMAllocsManager()->getSVMAlloc(ptr);
        if (svmEntry) {
            if ((svmEntry->gpuAllocation->getGpuAddress() + svmEntry->size) < (castToUint64(ptr) + size)) {
                return CL_INVALID_OPERATION;
            }

            mapAllocation = svmEntry->cpuAllocation ? svmEntry->cpuAllocation : svmEntry->gpuAllocation;
            if (isCpuCopyAllowed) {
                if (svmEntry->memoryType == DEVICE_UNIFIED_MEMORY) {
                    isCpuCopyAllowed = false;
                }
            }
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

    auto eBuiltInOps = EBuiltInOps::CopyBufferToBuffer;
    if (forceStateless(buffer->getSize())) {
        eBuiltInOps = EBuiltInOps::CopyBufferToBufferStateless;
    }
    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(eBuiltInOps,
                                                                            this->getDevice());
    BuiltInOwnershipWrapper builtInLock(builder, this->context);

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
            auto &csr = blitEnqueueAllowed(cmdType) ? *getBcsCommandStreamReceiver() : getGpgpuCommandStreamReceiver();
            bool status = csr.createAllocationForHostSurface(hostPtrSurf, true);
            if (!status) {
                return CL_OUT_OF_RESOURCES;
            }
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

    MultiDispatchInfo dispatchInfo;
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
