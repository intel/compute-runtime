/*
 * Copyright (C) 2018-2021 Intel Corporation
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
    auto eBuiltInOps = EBuiltInOps::CopyBufferRect;
    constexpr cl_command_type cmdType = CL_COMMAND_COPY_BUFFER_RECT;

    CsrSelectionArgs csrSelectionArgs{cmdType, srcBuffer, dstBuffer, device->getRootDeviceIndex(), region};
    CommandStreamReceiver &csr = selectCsrForBuiltinOperation(csrSelectionArgs);

    if (forceStateless(std::max(srcBuffer->getSize(), dstBuffer->getSize()))) {
        eBuiltInOps = EBuiltInOps::CopyBufferRectStateless;
    }

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

    MultiDispatchInfo dispatchInfo(dc);
    dispatchBcsOrGpgpuEnqueue<CL_COMMAND_COPY_BUFFER_RECT>(dispatchInfo, surfaces, eBuiltInOps, numEventsInWaitList, eventWaitList, event, false, csr);

    return CL_SUCCESS;
}
} // namespace NEO
