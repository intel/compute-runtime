/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/api/opencl/source/api/api.h"
#include "level_zero/api/opencl/source/helpers/base_object.h"
#include "level_zero/api/opencl/source/helpers/cl_validators.h"
#include "level_zero/api/opencl/source/mem_obj/buffer.h"
#include "level_zero/api/opencl/source/mem_obj/image.h"
#include "level_zero/api/opencl/source/platform/platform.h"
#include "level_zero/api/opencl/source/sharings/gl/gl_buffer.h"
#include "level_zero/api/opencl/source/sharings/gl/gl_sync_event.h"
#include "level_zero/api/opencl/source/sharings/gl/gl_texture.h"
#include "level_zero/api/opencl/source/tracing/tracing_notify.h"

#include "CL/cl.h"
#include "CL/cl_gl.h"
#include "config.h"

using namespace NEO::LEO;

cl_mem CL_API_CALL clCreateFromGLBuffer(cl_context context, cl_mem_flags flags, cl_GLuint bufobj, cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateFromGlBuffer, &context, &flags, &bufobj, &errcodeRet);
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    auto [retVal, pContext] = validateAndCast(std::make_tuple(context));
    if (retVal != CL_SUCCESS) {
        err.set(retVal);
        cl_mem tracingRetVal = nullptr;
        TRACING_EXIT(ClCreateFromGlBuffer, &tracingRetVal);
        return tracingRetVal;
    }

    if (pContext->getSharing<GLSharingFunctions>() == nullptr) {
        err.set(CL_INVALID_CONTEXT);
        cl_mem tracingRetVal = nullptr;
        TRACING_EXIT(ClCreateFromGlBuffer, &tracingRetVal);
        return tracingRetVal;
    }

    cl_mem tracingRetVal = GlBuffer::createSharedGlBuffer(pContext, flags, bufobj, errcodeRet);
    TRACING_EXIT(ClCreateFromGlBuffer, &tracingRetVal);
    return tracingRetVal;
}

cl_mem CL_API_CALL clCreateFromGLTexture(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel,
                                         cl_GLuint texture, cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateFromGlTexture, &context, &flags, &target, &miplevel, &texture, &errcodeRet);
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    auto [retVal, pContext] = validateAndCast(std::make_tuple(context));
    if (retVal != CL_SUCCESS) {
        err.set(retVal);
        cl_mem tracingRetVal = nullptr;
        TRACING_EXIT(ClCreateFromGlTexture, &tracingRetVal);
        return tracingRetVal;
    }

    if (pContext->getSharing<GLSharingFunctions>() == nullptr) {
        err.set(CL_INVALID_CONTEXT);
        cl_mem tracingRetVal = nullptr;
        TRACING_EXIT(ClCreateFromGlTexture, &tracingRetVal);
        return tracingRetVal;
    }

    cl_mem tracingRetVal = GlTexture::createSharedGlTexture(pContext, flags, target, miplevel, texture, errcodeRet);
    TRACING_EXIT(ClCreateFromGlTexture, &tracingRetVal);
    return tracingRetVal;
}

// deprecated OpenCL 1.1
cl_mem CL_API_CALL clCreateFromGLTexture2D(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel,
                                           cl_GLuint texture, cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateFromGlTexture2D, &context, &flags, &target, &miplevel, &texture, &errcodeRet);
    cl_mem tracingRetVal = clCreateFromGLTexture(context, flags, target, miplevel, texture, errcodeRet);
    TRACING_EXIT(ClCreateFromGlTexture2D, &tracingRetVal);
    return tracingRetVal;
}

// deprecated OpenCL 1.1
cl_mem CL_API_CALL clCreateFromGLTexture3D(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel,
                                           cl_GLuint texture, cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateFromGlTexture3D, &context, &flags, &target, &miplevel, &texture, &errcodeRet);
    cl_mem tracingRetVal = clCreateFromGLTexture(context, flags, target, miplevel, texture, errcodeRet);
    TRACING_EXIT(ClCreateFromGlTexture3D, &tracingRetVal);
    return tracingRetVal;
}

cl_mem CL_API_CALL clCreateFromGLRenderbuffer(cl_context context, cl_mem_flags flags, cl_GLuint renderbuffer, cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateFromGlRenderbuffer, &context, &flags, &renderbuffer, &errcodeRet);
    cl_mem tracingRetVal = clCreateFromGLTexture(context, flags, GL_RENDERBUFFER_EXT, 0, renderbuffer, errcodeRet);
    TRACING_EXIT(ClCreateFromGlRenderbuffer, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clGetGLObjectInfo(cl_mem memobj, cl_gl_object_type *glObjectType, cl_GLuint *glObjectName) {
    TRACING_ENTER(ClGetGlObjectInfo, &memobj, &glObjectType, &glObjectName);
    auto [retVal, pMemObj] = validateAndCast(std::make_tuple(memobj));
    if (retVal != CL_SUCCESS) {
        cl_int tracingRetVal = CL_INVALID_GL_OBJECT;
        TRACING_EXIT(ClGetGlObjectInfo, &tracingRetVal);
        return tracingRetVal;
    }

    auto handler = static_cast<GlSharing *>(pMemObj->peekSharingHandler());
    if (!handler) {
        cl_int tracingRetVal = CL_INVALID_GL_OBJECT;
        TRACING_EXIT(ClGetGlObjectInfo, &tracingRetVal);
        return tracingRetVal;
    }

    handler->getGlObjectInfo(glObjectType, glObjectName);

    cl_int tracingRetVal = CL_SUCCESS;
    TRACING_EXIT(ClGetGlObjectInfo, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clGetGLTextureInfo(cl_mem memobj, cl_gl_texture_info paramName, size_t paramValueSize, void *paramValue,
                                      size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetGlTextureInfo, &memobj, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    auto [retVal, pMemObj] = validateAndCast(std::make_tuple(memobj));
    if (retVal != CL_SUCCESS) {
        cl_int tracingRetVal = CL_INVALID_GL_OBJECT;
        TRACING_EXIT(ClGetGlTextureInfo, &tracingRetVal);
        return tracingRetVal;
    }

    auto handler = static_cast<GlTexture *>(pMemObj->peekSharingHandler());
    if (!handler) {
        cl_int tracingRetVal = CL_INVALID_GL_OBJECT;
        TRACING_EXIT(ClGetGlTextureInfo, &tracingRetVal);
        return tracingRetVal;
    }

    cl_int tracingRetVal = handler->getGlTextureInfo(paramName, paramValueSize, paramValue, paramValueSizeRet);
    TRACING_EXIT(ClGetGlTextureInfo, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clEnqueueAcquireGLObjects(cl_command_queue commandQueue, cl_uint numObjects, const cl_mem *memObjects,
                                             cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) {
    TRACING_ENTER(ClEnqueueAcquireGlObjects, &commandQueue, &numObjects, &memObjects, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue] = validateAndCast(std::make_tuple(commandQueue));
    if (retVal != CL_SUCCESS) {
        TRACING_EXIT(ClEnqueueAcquireGlObjects, &retVal);
        return retVal;
    }

    if (pCommandQueue->getContext()->getSharing<GLSharingFunctions>() == nullptr) {
        cl_int tracingRetVal = CL_INVALID_CONTEXT;
        TRACING_EXIT(ClEnqueueAcquireGlObjects, &tracingRetVal);
        return tracingRetVal;
    }

    cl_int tracingRetVal = pCommandQueue->enqueueAcquireSharedObjects(numObjects, memObjects, numEventsInWaitList, eventWaitList, event, CL_COMMAND_ACQUIRE_GL_OBJECTS);
    TRACING_EXIT(ClEnqueueAcquireGlObjects, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clEnqueueReleaseGLObjects(cl_command_queue commandQueue, cl_uint numObjects, const cl_mem *memObjects,
                                             cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) {
    TRACING_ENTER(ClEnqueueReleaseGlObjects, &commandQueue, &numObjects, &memObjects, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue] = validateAndCast(std::make_tuple(commandQueue));
    if (retVal != CL_SUCCESS) {
        TRACING_EXIT(ClEnqueueReleaseGlObjects, &retVal);
        return retVal;
    }

    if (pCommandQueue->getContext()->getSharing<GLSharingFunctions>() == nullptr) {
        cl_int tracingRetVal = CL_INVALID_CONTEXT;
        TRACING_EXIT(ClEnqueueReleaseGlObjects, &tracingRetVal);
        return tracingRetVal;
    }

    cl_int tracingRetVal = pCommandQueue->enqueueReleaseSharedObjects(numObjects, memObjects, numEventsInWaitList, eventWaitList, event, CL_COMMAND_RELEASE_GL_OBJECTS);
    TRACING_EXIT(ClEnqueueReleaseGlObjects, &tracingRetVal);
    return tracingRetVal;
}

cl_event CL_API_CALL clCreateEventFromGLsyncKHR(cl_context context, cl_GLsync sync, cl_int *errcodeRet) {
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    auto [retVal, pContext] = validateAndCast(std::make_tuple(context));
    if (retVal != CL_SUCCESS) {
        err.set(retVal);
        return nullptr;
    }

    if (pContext->getSharing<GLSharingFunctions>() == nullptr) {
        err.set(CL_INVALID_CONTEXT);
        return nullptr;
    }

    return GlSyncEvent::create(*pContext, sync, errcodeRet);
}

cl_int CL_API_CALL clGetGLContextInfoKHR(const cl_context_properties *properties, cl_gl_context_info paramName, size_t paramValueSize,
                                         void *paramValue, size_t *paramValueSizeRet) {
    GetInfoHelper info(paramValue, paramValueSize, paramValueSizeRet);

    uint32_t glHglrcHandle = 0;
    uint32_t glHdcHandle = 0;
    uint32_t propertyType = 0;
    uint32_t propertyValue = 0;
    Platform *platform = nullptr;

    if (properties != nullptr) {
        while (*properties != 0) {
            propertyType = static_cast<uint32_t>(properties[0]);
            propertyValue = static_cast<uint32_t>(properties[1]);
            switch (propertyType) {
            case CL_CONTEXT_PLATFORM: {
                platform = castToObject<Platform>(reinterpret_cast<cl_platform_id>(properties[1]));
            } break;
            case CL_GL_CONTEXT_KHR:
                glHglrcHandle = propertyValue;
                break;
            case CL_WGL_HDC_KHR:
                glHdcHandle = propertyValue;
                break;
            }
            properties += 2;
        }
    }

    auto glSharing = GLSharingFunctions::create();

    if ((glHglrcHandle == 0) || glSharing->isGlHdcHandleMissing(glHdcHandle)) {
        return CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR;
    }

    glSharing->initGLFunctions();
    if (glSharing->isOpenGlSharingSupported() == false) {
        return CL_INVALID_CONTEXT;
    }

    if (paramName == CL_DEVICES_FOR_GL_CONTEXT_KHR || paramName == CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR) {
        if (!platform) {
            platform = (*platformsImpl)[0].get();
        }

        ClDevice *deviceToReturn = nullptr;
        for (auto i = 0u; i < std::ssize(platform->getDevices()); i++) {
            auto device = platform->getDevices()[i].get();
            if (glSharing->isHandleCompatible(*device->getDevice().getRootDeviceEnvironment().osInterface->getDriverModel(), glHglrcHandle)) {
                deviceToReturn = device;
                break;
            }
        }
        if (!deviceToReturn) {
            return CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR;
        }

        info.set<cl_device_id>(deviceToReturn);
        return CL_SUCCESS;
    }

    return CL_INVALID_VALUE;
}

cl_int CL_API_CALL clGetSupportedGLTextureFormatsINTEL(
    cl_context context,
    cl_mem_flags flags,
    cl_mem_object_type imageType,
    cl_uint numEntries,
    cl_GLenum *glFormats,
    cl_uint *numTextureFormats) {

    if (numTextureFormats) {
        *numTextureFormats = 0;
    }

    auto [retVal, pContext] = validateAndCast(std::make_tuple(context));
    if (retVal != CL_SUCCESS) {
        return retVal;
    }

    auto pSharing = pContext->getSharing<GLSharingFunctions>();
    if (!pSharing) {
        return CL_INVALID_CONTEXT;
    }

    return pSharing->getSupportedFormats(flags, imageType, numEntries, glFormats, numTextureFormats);
}
