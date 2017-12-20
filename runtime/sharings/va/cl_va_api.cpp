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

#include "config.h"
#include "runtime/api/api.h"
#include "CL/cl.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/context/context.h"
#include "runtime/helpers/get_info.h"
#include "runtime/sharings/va/va_sharing.h"
#include "runtime/sharings/va/va_surface.h"
#include <cstring>

using namespace OCLRT;

cl_mem CL_API_CALL
clCreateFromVA_APIMediaSurfaceINTEL(cl_context context, cl_mem_flags flags, VASurfaceID *surface,
                                    cl_uint plane, cl_int *errcodeRet) {

    Context *pContext = nullptr;

    auto returnCode = validateObject(WithCastToInternal(context, &pContext));

    ErrorCodeHelper err(errcodeRet, returnCode);

    if (returnCode != CL_SUCCESS) {
        return nullptr;
    }

    return VASurface::createSharedVaSurface(pContext, pContext->getSharing<VASharingFunctions>(), flags, surface, plane, errcodeRet);
}

cl_int CL_API_CALL
clEnqueueAcquireVA_APIMediaSurfacesINTEL(cl_command_queue commandQueue,
                                         cl_uint numObjects,
                                         const cl_mem *memObjects,
                                         cl_uint numEventsInWaitList,
                                         const cl_event *eventWaitList,
                                         cl_event *event) {

    CommandQueue *pCommandQueue = nullptr;

    auto status = validateObjects(WithCastToInternal(commandQueue, &pCommandQueue));

    if (status == CL_SUCCESS) {
        status = pCommandQueue->enqueueAcquireSharedObjects(numObjects, memObjects, numEventsInWaitList,
                                                            eventWaitList, event, CL_COMMAND_ACQUIRE_VA_API_MEDIA_SURFACES_INTEL);
    }
    return status;
}

cl_int CL_API_CALL
clEnqueueReleaseVA_APIMediaSurfacesINTEL(cl_command_queue commandQueue,
                                         cl_uint numObjects,
                                         const cl_mem *memObjects,
                                         cl_uint numEventsInWaitList,
                                         const cl_event *eventWaitList,
                                         cl_event *event) {

    CommandQueue *pCommandQueue = nullptr;

    auto status = validateObjects(WithCastToInternal(commandQueue, &pCommandQueue));

    if (status == CL_SUCCESS) {
        status = pCommandQueue->enqueueReleaseSharedObjects(numObjects, memObjects, numEventsInWaitList,
                                                            eventWaitList, event, CL_COMMAND_RELEASE_VA_API_MEDIA_SURFACES_INTEL);
    }
    return status;
}
