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
#include "runtime/helpers/kernel_commands.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/mem_obj/image.h"
#include <algorithm>
#include <new>

namespace OCLRT {

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueWriteImage(
    Image *dstImage,
    cl_bool blockingWrite,
    const size_t *origin,
    const size_t *region,
    size_t inputRowPitch,
    size_t inputSlicePitch,
    const void *ptr,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {

    MultiDispatchInfo di;

    auto &builder = BuiltIns::getInstance().getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToImage3d,
                                                                          this->getContext(), this->getDevice());

    builder.takeOwnership(this->context);

    BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
    dc.srcPtr = const_cast<void *>(ptr);
    dc.dstMemObj = dstImage;
    dc.dstOffset = origin;
    dc.size = region;
    dc.dstRowPitch = inputRowPitch;
    dc.dstSlicePitch = inputSlicePitch;
    builder.buildDispatchInfos(di, dc);

    enqueueHandler<CL_COMMAND_WRITE_IMAGE>(
        di.getUsedSurfaces().begin(),
        di.getUsedSurfaces().size(),
        blockingWrite == CL_TRUE,
        di,
        numEventsInWaitList,
        eventWaitList,
        event);

    builder.releaseOwnership();

    if (context->isProvidingPerformanceHints()) {
        context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_NEUTRAL_INTEL, CL_ENQUEUE_WRITE_IMAGE_REQUIRES_COPY_DATA, static_cast<cl_mem>(dstImage));
    }

    return CL_SUCCESS;
}
}
