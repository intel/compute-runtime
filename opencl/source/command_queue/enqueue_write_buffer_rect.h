/*
 * Copyright (C) 2018-2025 Intel Corporation
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

    CsrSelectionArgs csrSelectionArgs{cmdType, buffer, {}, device->getRootDeviceIndex(), region};
    CommandStreamReceiver &csr = selectCsrForBuiltinOperation(csrSelectionArgs);

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

    const size_t hostPtrSize = Buffer::calculateHostPtrSize(hostOrigin, region, hostRowPitch, hostSlicePitch);
    const uint32_t rootDeviceIndex = getDevice().getRootDeviceIndex();
    InternalMemoryType memoryType = InternalMemoryType::notSpecified;
    GraphicsAllocation *mapAllocation = nullptr;
    bool isCpuCopyAllowed = false;
    getContext().tryGetExistingHostPtrAllocation(ptr, hostPtrSize, rootDeviceIndex, mapAllocation, memoryType, isCpuCopyAllowed);

    const bool isStateless = forceStateless(buffer->getSize());
    const bool useHeapless = this->getHeaplessModeEnabled();
    auto builtInType = EBuiltInOps::adjustBuiltinType<EBuiltInOps::copyBufferRect>(isStateless, useHeapless);

    void *srcPtr = const_cast<void *>(ptr);

    MemObjSurface dstBufferSurf(buffer);
    HostPtrSurface hostPtrSurf(srcPtr, hostPtrSize, true);
    GeneralSurface mapSurface;
    Surface *surfaces[] = {&dstBufferSurf, nullptr};

    auto bcsSplit = this->isSplitEnqueueBlitNeeded(csrSelectionArgs.direction, getTotalSizeFromRectRegion(region), csr);

    if (region[0] != 0 && region[1] != 0 && region[2] != 0) {
        if (mapAllocation) {
            surfaces[1] = &mapSurface;
            mapSurface.setGraphicsAllocation(mapAllocation);
            srcPtr = convertAddressWithOffsetToGpuVa(srcPtr, memoryType, *mapAllocation);
        } else {
            surfaces[1] = &hostPtrSurf;
            bool status = selectCsrForHostPtrAllocation(bcsSplit, csr).createAllocationForHostSurface(hostPtrSurf, false);
            if (!status) {
                return CL_OUT_OF_RESOURCES;
            }
            this->prepareHostPtrSurfaceForSplit(bcsSplit, *hostPtrSurf.getAllocation());

            srcPtr = reinterpret_cast<void *>(hostPtrSurf.getAllocation()->getGpuAddress());
        }
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
    dc.bcsSplit = bcsSplit;
    dc.direction = csrSelectionArgs.direction;

    MultiDispatchInfo dispatchInfo(dc);
    const auto dispatchResult = dispatchBcsOrGpgpuEnqueue<CL_COMMAND_WRITE_BUFFER_RECT>(dispatchInfo, surfaces, builtInType, numEventsInWaitList, eventWaitList, event, blockingWrite, csr);
    if (dispatchResult != CL_SUCCESS) {
        return dispatchResult;
    }

    if (context->isProvidingPerformanceHints()) {
        context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_NEUTRAL_INTEL, CL_ENQUEUE_WRITE_BUFFER_RECT_REQUIRES_COPY_DATA, static_cast<cl_mem>(buffer));
    }

    return CL_SUCCESS;
}
} // namespace NEO
