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
#include "runtime/built_ins/built_ins.h"
#include "hw_cmds.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/surface_formats.h"
#include "runtime/helpers/cache_policy.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/surface.h"
#include <algorithm>
#include <new>

namespace OCLRT {

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueReadImage(
    Image *srcImage,
    cl_bool blockingRead,
    const size_t *origin,
    const size_t *region,
    size_t inputRowPitch,
    size_t inputSlicePitch,
    void *ptr,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {

    MultiDispatchInfo di;

    size_t hostOffset;
    Image::calculateHostPtrOffset(&hostOffset, origin, region, inputRowPitch, inputSlicePitch, srcImage->getImageDesc().image_type, srcImage->getSurfaceFormatInfo().ImageElementSizeInBytes);
    auto isMemTransferNeeded = srcImage->checkIfMemoryTransferIsRequired(hostOffset, 0, ptr, CL_COMMAND_READ_IMAGE);
    if (!isMemTransferNeeded) {
        NullSurface s;
        Surface *surfaces[] = {&s};
        enqueueHandler<CL_COMMAND_MARKER>(
            surfaces,
            blockingRead == CL_TRUE,
            di,
            numEventsInWaitList,
            eventWaitList,
            event);
        if (event) {
            auto pEvent = castToObjectOrAbort<Event>(*event);
            pEvent->setCmdType(CL_COMMAND_READ_IMAGE);
        }

        if (context->isProvidingPerformanceHints()) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, CL_ENQUEUE_READ_IMAGE_DOESNT_REQUIRES_COPY_DATA, static_cast<cl_mem>(srcImage));
        }

        return CL_SUCCESS;
    }

    auto &builder = BuiltIns::getInstance().getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyImage3dToBuffer,
                                                                          this->getContext(), this->getDevice());

    builder.takeOwnership(this->context);

    BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
    dc.srcMemObj = srcImage;
    dc.dstPtr = ptr;
    dc.srcOffset = origin;
    dc.size = region;
    dc.srcRowPitch = inputRowPitch;
    dc.srcSlicePitch = inputSlicePitch;
    builder.buildDispatchInfos(di, dc);

    enqueueHandler<CL_COMMAND_READ_IMAGE>(
        di.getUsedSurfaces().begin(),
        di.getUsedSurfaces().size(),
        blockingRead == CL_TRUE,
        di,
        numEventsInWaitList,
        eventWaitList,
        event);

    builder.releaseOwnership();

    if (context->isProvidingPerformanceHints()) {
        HostPtrSurface *hps = di.getHostPtrSurface();
        DEBUG_BREAK_IF(!((hps != nullptr) && (hps->getMemoryPointer() == ptr)));
        if (!isL3Capable(hps->getMemoryPointer(), hps->getSurfaceSize())) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, CL_ENQUEUE_READ_IMAGE_DOESNT_MEET_ALIGNMENT_RESTRICTIONS, hps->getMemoryPointer(), hps->getSurfaceSize(), MemoryConstants::pageSize, MemoryConstants::pageSize);
        }
    }

    return CL_SUCCESS;
}
} // namespace OCLRT
