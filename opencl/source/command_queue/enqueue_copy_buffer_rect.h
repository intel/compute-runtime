/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/memory_manager/mem_obj_surface.h"

namespace NEO {

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueCopyBufferRect(
    Buffer *srcBuffer,
    Buffer *dstBuffer,
    const size_t *srcOrigin,
    const size_t *dstOrigin,
    const size_t *region,
    size_t srcRowPitch,
    size_t srcSlicePitch,
    size_t dstRowPitch,
    size_t dstSlicePitch,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {

    constexpr cl_command_type cmdType = CL_COMMAND_COPY_BUFFER_RECT;

    CsrSelectionArgs csrSelectionArgs{cmdType, srcBuffer, dstBuffer, device->getRootDeviceIndex(), region};
    CommandStreamReceiver &csr = selectCsrForBuiltinOperation(csrSelectionArgs);

    const bool isStateless = isForceStateless || forceStateless(std::max(srcBuffer->getSize(), dstBuffer->getSize()));
    const bool useHeapless = this->getHeaplessModeEnabled();
    auto builtInType = EBuiltInOps::adjustBuiltinType<EBuiltInOps::copyBufferRect>(isStateless, useHeapless);

    MemObjSurface srcBufferSurf(srcBuffer);
    MemObjSurface dstBufferSurf(dstBuffer);
    Surface *surfaces[] = {&srcBufferSurf, &dstBufferSurf};

    BuiltinOpParams dc;
    dc.srcMemObj = srcBuffer;
    dc.dstMemObj = dstBuffer;
    dc.srcOffset = srcOrigin;
    dc.dstOffset = dstOrigin;
    dc.size = region;
    dc.srcRowPitch = srcRowPitch;
    dc.srcSlicePitch = srcSlicePitch;
    dc.dstRowPitch = dstRowPitch;
    dc.dstSlicePitch = dstSlicePitch;
    dc.bcsSplit = this->isSplitEnqueueBlitNeeded(csrSelectionArgs.direction, getTotalSizeFromRectRegion(region), csr);
    dc.direction = csrSelectionArgs.direction;

    MultiDispatchInfo dispatchInfo(dc);
    return dispatchBcsOrGpgpuEnqueue<CL_COMMAND_COPY_BUFFER_RECT>(dispatchInfo, surfaces, builtInType, numEventsInWaitList, eventWaitList, event, false, csr);
}

} // namespace NEO
