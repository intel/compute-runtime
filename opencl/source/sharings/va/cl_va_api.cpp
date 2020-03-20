/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/get_info.h"
#include "shared/source/utilities/api_intercept.h"

#include "opencl/source/api/api.h"
#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/sharings/va/va_sharing.h"
#include "opencl/source/sharings/va/va_surface.h"

#include "CL/cl.h"

#include <cstring>

using namespace NEO;

cl_mem CL_API_CALL
clCreateFromVA_APIMediaSurfaceINTEL(cl_context context, cl_mem_flags flags, VASurfaceID *surface,
                                    cl_uint plane, cl_int *errcodeRet) {

    cl_int returnCode = CL_SUCCESS;
    API_ENTER(&returnCode);
    DBG_LOG_INPUTS("context", context,
                   "flags", flags,
                   "VASurfaceID", surface,
                   "plane", plane);

    Context *pContext = nullptr;
    cl_mem image = nullptr;

    returnCode = validateObject(WithCastToInternal(context, &pContext));
    ErrorCodeHelper err(errcodeRet, returnCode);

    if (returnCode != CL_SUCCESS) {
        return nullptr;
    }

    if (!VASurface::validate(flags, plane)) {
        returnCode = CL_INVALID_VALUE;
        err.set(returnCode);
        return nullptr;
    }

    image = VASurface::createSharedVaSurface(pContext, pContext->getSharing<VASharingFunctions>(), flags, surface, plane, errcodeRet);
    DBG_LOG_INPUTS("image", image);
    return image;
}

cl_int CL_API_CALL
clGetDeviceIDsFromVA_APIMediaAdapterINTEL(cl_platform_id platform, cl_va_api_device_source_intel mediaAdapterType,
                                          void *mediaAdapter, cl_va_api_device_set_intel mediaAdapterSet, cl_uint numEntries,
                                          cl_device_id *devices, cl_uint *numDevices) {
    cl_int status = CL_SUCCESS;
    API_ENTER(&status);
    DBG_LOG_INPUTS("platform", platform,
                   "mediaAdapterType", mediaAdapterType,
                   "mediaAdapter", mediaAdapter,
                   "mediaAdapterSet", mediaAdapterSet,
                   "numEntries", numEntries);

    Platform *pPlatform = nullptr;
    status = validateObjects(WithCastToInternal(platform, &pPlatform));
    if (status != CL_SUCCESS) {
        status = CL_INVALID_PLATFORM;
    } else {
        cl_device_id device = pPlatform->getClDevice(0);
        GetInfoHelper::set(devices, device);
        GetInfoHelper::set(numDevices, 1u);
    }
    return status;
}

cl_int CL_API_CALL
clEnqueueAcquireVA_APIMediaSurfacesINTEL(cl_command_queue commandQueue,
                                         cl_uint numObjects,
                                         const cl_mem *memObjects,
                                         cl_uint numEventsInWaitList,
                                         const cl_event *eventWaitList,
                                         cl_event *event) {
    cl_int status = CL_SUCCESS;
    API_ENTER(&status);
    DBG_LOG_INPUTS("commandQueue", commandQueue,
                   "numObjects", numObjects,
                   "memObjects", FileLoggerInstance().getMemObjects(reinterpret_cast<const uintptr_t *>(memObjects), numObjects),
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", FileLoggerInstance().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", FileLoggerInstance().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    CommandQueue *pCommandQueue = nullptr;

    status = validateObjects(WithCastToInternal(commandQueue, &pCommandQueue));

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
    cl_int status = CL_SUCCESS;
    API_ENTER(&status);
    DBG_LOG_INPUTS("commandQueue", commandQueue,
                   "numObjects", numObjects,
                   "memObjects", FileLoggerInstance().getMemObjects(reinterpret_cast<const uintptr_t *>(memObjects), numObjects),
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", FileLoggerInstance().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", FileLoggerInstance().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    CommandQueue *pCommandQueue = nullptr;

    status = validateObjects(WithCastToInternal(commandQueue, &pCommandQueue));

    if (status == CL_SUCCESS) {
        status = pCommandQueue->enqueueReleaseSharedObjects(numObjects, memObjects, numEventsInWaitList,
                                                            eventWaitList, event, CL_COMMAND_RELEASE_VA_API_MEDIA_SURFACES_INTEL);
    }
    return status;
}

cl_int CL_API_CALL clGetSupportedVA_APIMediaSurfaceFormatsINTEL(
    cl_context context,
    cl_mem_flags flags,
    cl_mem_object_type imageType,
    cl_uint numEntries,
    VAImageFormat *vaApiFormats,
    cl_uint *numImageFormats) {

    if (numImageFormats) {
        *numImageFormats = 0;
    }

    Context *pContext = castToObjectOrAbort<Context>(context);
    auto pSharing = pContext->getSharing<VASharingFunctions>();
    if (!pSharing) {
        return CL_INVALID_CONTEXT;
    }

    return pSharing->getSupportedFormats(flags, imageType, numEntries, vaApiFormats, numImageFormats);
}
