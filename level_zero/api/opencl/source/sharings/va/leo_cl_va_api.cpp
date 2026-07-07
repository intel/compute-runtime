/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/sharings/va/leo_cl_va_api.h"

#include "shared/source/helpers/get_info.h"

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_validators.h"
#include "level_zero/api/opencl/source/mem_obj/leo_image.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/source/sharings/va/leo_va_device.h"
#include "level_zero/api/opencl/source/sharings/va/leo_va_surface.h"

#include "CL/cl.h"

using namespace NEO::LEO;

cl_mem CL_API_CALL
clCreateFromVA_APIMediaSurfaceINTEL(cl_context context, cl_mem_flags flags, VASurfaceID *surface,
                                    cl_uint plane, cl_int *errcodeRet) {
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    Context *pContext = castToObject<Context>(context);
    if (!pContext) {
        err.set(CL_INVALID_CONTEXT);
        return nullptr;
    }

    if (!VASurface::validate(flags, plane)) {
        err.set(CL_INVALID_VALUE);
        return nullptr;
    }

    return VASurface::createSharedVaSurface(pContext, pContext->getSharing<VASharingFunctions>(), flags, 0, surface, plane, errcodeRet);
}

cl_int CL_API_CALL
clGetDeviceIDsFromVA_APIMediaAdapterINTEL(cl_platform_id platform, cl_va_api_device_source_intel mediaAdapterType,
                                          void *mediaAdapter, cl_va_api_device_set_intel mediaAdapterSet, cl_uint numEntries,
                                          cl_device_id *devices, cl_uint *numDevices) {
    Platform *pPlatform = castToObject<Platform>(platform);
    if (!pPlatform) {
        return CL_INVALID_PLATFORM;
    }

    NEO::VADevice vaDevice{};
    auto device = vaDevice.getDeviceFromVA(reinterpret_cast<NEO::Platform *>(pPlatform), mediaAdapter);
    GetInfoHelper::set(devices, reinterpret_cast<cl_device_id>(device));
    if (device == nullptr) {
        GetInfoHelper::set(numDevices, 0u);
        return CL_DEVICE_NOT_FOUND;
    } else {
        GetInfoHelper::set(numDevices, 1u);
    }

    return CL_SUCCESS;
}

cl_int CL_API_CALL
clEnqueueAcquireVA_APIMediaSurfacesINTEL(cl_command_queue commandQueue,
                                         cl_uint numObjects,
                                         const cl_mem *memObjects,
                                         cl_uint numEventsInWaitList,
                                         const cl_event *eventWaitList,
                                         cl_event *event) {
    auto pCommandQueue = castToObject<CommandQueue>(commandQueue);

    if (!pCommandQueue) {
        return CL_INVALID_COMMAND_QUEUE;
    }

    auto ret = pCommandQueue->enqueueAcquireSharedObjects(numObjects, memObjects, numEventsInWaitList, eventWaitList, event, CL_COMMAND_ACQUIRE_VA_API_MEDIA_SURFACES_INTEL);

    if (ret != CL_SUCCESS) {
        return ret;
    }

    if (!pCommandQueue->getContext()->getInteropUserSyncEnabled()) {
        ret = clFinish(commandQueue);
    }

    return ret;
}

cl_int CL_API_CALL
clEnqueueReleaseVA_APIMediaSurfacesINTEL(cl_command_queue commandQueue,
                                         cl_uint numObjects,
                                         const cl_mem *memObjects,
                                         cl_uint numEventsInWaitList,
                                         const cl_event *eventWaitList,
                                         cl_event *event) {
    auto pCommandQueue = castToObject<CommandQueue>(commandQueue);

    if (!pCommandQueue) {
        return CL_INVALID_COMMAND_QUEUE;
    }

    auto ret = pCommandQueue->enqueueReleaseSharedObjects(numObjects, memObjects, numEventsInWaitList, eventWaitList, event, CL_COMMAND_RELEASE_VA_API_MEDIA_SURFACES_INTEL);

    if (ret != CL_SUCCESS) {
        return ret;
    }

    if (!pCommandQueue->getContext()->getInteropUserSyncEnabled()) {
        ret = clFinish(commandQueue);
    }

    return ret;
}

cl_int CL_API_CALL clGetSupportedVA_APIMediaSurfaceFormatsINTEL(
    cl_context context,
    cl_mem_flags flags,
    cl_mem_object_type imageType,
    cl_uint plane,
    cl_uint numEntries,
    VAImageFormat *vaApiFormats,
    cl_uint *numImageFormats) {

    if (numImageFormats) {
        *numImageFormats = 0;
    }

    Context *pContext = castToObject<Context>(context);
    auto pSharing = pContext->getSharing<VASharingFunctions>();
    if (!pSharing) {
        return CL_INVALID_CONTEXT;
    }

    return pSharing->getSupportedFormats(flags, imageType, plane, numEntries, vaApiFormats, numImageFormats);
}
