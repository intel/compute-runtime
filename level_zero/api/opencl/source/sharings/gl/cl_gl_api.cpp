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

#include "CL/cl.h"
#include "CL/cl_gl.h"
#include "config.h"

using namespace NEO::LEO;

cl_mem CL_API_CALL clCreateFromGLBuffer(cl_context context, cl_mem_flags flags, cl_GLuint bufobj, cl_int *errcodeRet) {
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

    return GlBuffer::createSharedGlBuffer(pContext, flags, bufobj, errcodeRet);
}

cl_mem CL_API_CALL clCreateFromGLTexture(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel,
                                         cl_GLuint texture, cl_int *errcodeRet) {
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

    return GlTexture::createSharedGlTexture(pContext, flags, target, miplevel, texture, errcodeRet);
}

// deprecated OpenCL 1.1
cl_mem CL_API_CALL clCreateFromGLTexture2D(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel,
                                           cl_GLuint texture, cl_int *errcodeRet) {
    return clCreateFromGLTexture(context, flags, target, miplevel, texture, errcodeRet);
}

// deprecated OpenCL 1.1
cl_mem CL_API_CALL clCreateFromGLTexture3D(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel,
                                           cl_GLuint texture, cl_int *errcodeRet) {
    return clCreateFromGLTexture(context, flags, target, miplevel, texture, errcodeRet);
}

cl_mem CL_API_CALL clCreateFromGLRenderbuffer(cl_context context, cl_mem_flags flags, cl_GLuint renderbuffer, cl_int *errcodeRet) {
    return clCreateFromGLTexture(context, flags, GL_RENDERBUFFER_EXT, 0, renderbuffer, errcodeRet);
}

cl_int CL_API_CALL clGetGLObjectInfo(cl_mem memobj, cl_gl_object_type *glObjectType, cl_GLuint *glObjectName) {
    auto [retVal, pMemObj] = validateAndCast(std::make_tuple(memobj));
    if (retVal != CL_SUCCESS) {
        return CL_INVALID_GL_OBJECT;
    }

    auto handler = static_cast<GlSharing *>(pMemObj->peekSharingHandler());
    if (!handler) {
        return CL_INVALID_GL_OBJECT;
    }

    handler->getGlObjectInfo(glObjectType, glObjectName);

    return CL_SUCCESS;
}

cl_int CL_API_CALL clGetGLTextureInfo(cl_mem memobj, cl_gl_texture_info paramName, size_t paramValueSize, void *paramValue,
                                      size_t *paramValueSizeRet) {
    auto [retVal, pMemObj] = validateAndCast(std::make_tuple(memobj));
    if (retVal != CL_SUCCESS) {
        return CL_INVALID_GL_OBJECT;
    }

    auto handler = static_cast<GlTexture *>(pMemObj->peekSharingHandler());
    if (!handler) {
        return CL_INVALID_GL_OBJECT;
    }

    return handler->getGlTextureInfo(paramName, paramValueSize, paramValue, paramValueSizeRet);
}

cl_int CL_API_CALL clEnqueueAcquireGLObjects(cl_command_queue commandQueue, cl_uint numObjects, const cl_mem *memObjects,
                                             cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) {
    auto [retVal, pCommandQueue] = validateAndCast(std::make_tuple(commandQueue));
    if (retVal != CL_SUCCESS) {
        return retVal;
    }

    if (pCommandQueue->getContext()->getSharing<GLSharingFunctions>() == nullptr) {
        return CL_INVALID_CONTEXT;
    }

    return pCommandQueue->enqueueAcquireSharedObjects(numObjects, memObjects, numEventsInWaitList, eventWaitList, event, CL_COMMAND_ACQUIRE_GL_OBJECTS);
}

cl_int CL_API_CALL clEnqueueReleaseGLObjects(cl_command_queue commandQueue, cl_uint numObjects, const cl_mem *memObjects,
                                             cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) {
    auto [retVal, pCommandQueue] = validateAndCast(std::make_tuple(commandQueue));
    if (retVal != CL_SUCCESS) {
        return retVal;
    }

    if (pCommandQueue->getContext()->getSharing<GLSharingFunctions>() == nullptr) {
        return CL_INVALID_CONTEXT;
    }

    return pCommandQueue->enqueueReleaseSharedObjects(numObjects, memObjects, numEventsInWaitList, eventWaitList, event, CL_COMMAND_RELEASE_GL_OBJECTS);
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
