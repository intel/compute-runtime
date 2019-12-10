/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/api/api.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/event/async_events_handler.h"
#include "runtime/helpers/base_object.h"
#include "runtime/helpers/get_info.h"
#include "runtime/helpers/validators.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/image.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/platform/platform.h"
#include "runtime/sharings/gl/gl_buffer.h"
#include "runtime/sharings/gl/gl_sharing.h"
#include "runtime/sharings/gl/gl_sync_event.h"
#include "runtime/sharings/gl/gl_texture.h"
#include "runtime/tracing/tracing_notify.h"
#include "runtime/utilities/api_intercept.h"

#include "CL/cl.h"
#include "CL/cl_gl.h"
#include "config.h"

using namespace NEO;

cl_mem CL_API_CALL clCreateFromGLBuffer(cl_context context, cl_mem_flags flags, cl_GLuint bufobj, cl_int *errcodeRet) {
    TRACING_ENTER(clCreateFromGLBuffer, &context, &flags, &bufobj, &errcodeRet);

    API_ENTER(errcodeRet);
    DBG_LOG_INPUTS("context", context, "flags", flags, "bufobj", bufobj);

    Context *pContext = nullptr;

    auto returnCode = validateObjects(WithCastToInternal(context, &pContext));

    ErrorCodeHelper err(errcodeRet, returnCode);

    if (returnCode != CL_SUCCESS) {
        cl_mem buffer = nullptr;
        TRACING_EXIT(clCreateFromGLBuffer, &buffer);
        return buffer;
    }

    if (pContext->getSharing<GLSharingFunctions>() == nullptr) {
        err.set(CL_INVALID_CONTEXT);
        cl_mem buffer = nullptr;
        TRACING_EXIT(clCreateFromGLBuffer, &buffer);
        return buffer;
    }

    cl_mem buffer = GlBuffer::createSharedGlBuffer(pContext, flags, bufobj, errcodeRet);
    TRACING_EXIT(clCreateFromGLBuffer, &buffer);
    return buffer;
}

cl_mem CL_API_CALL clCreateFromGLTexture(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel,
                                         cl_GLuint texture, cl_int *errcodeRet) {
    TRACING_ENTER(clCreateFromGLTexture, &context, &flags, &target, &miplevel, &texture, &errcodeRet);

    API_ENTER(errcodeRet);
    DBG_LOG_INPUTS("context", context, "flags", flags, "target", target, "miplevel", miplevel, "texture", texture);
    Context *pContext = nullptr;

    auto returnCode = validateObjects(WithCastToInternal(context, &pContext));

    ErrorCodeHelper err(errcodeRet, returnCode);
    cl_mem image = nullptr;

    if (returnCode != CL_SUCCESS) {
        TRACING_EXIT(clCreateFromGLTexture, &image);
        return image;
    }

    if (pContext->getSharing<GLSharingFunctions>() == nullptr) {
        err.set(CL_INVALID_CONTEXT);
        TRACING_EXIT(clCreateFromGLTexture, &image);
        return image;
    }
    image = GlTexture::createSharedGlTexture(pContext, flags, target, miplevel, texture, errcodeRet);
    DBG_LOG_INPUTS("image", image);
    TRACING_EXIT(clCreateFromGLTexture, &image);
    return image;
}

// deprecated OpenCL 1.1
cl_mem CL_API_CALL clCreateFromGLTexture2D(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel,
                                           cl_GLuint texture, cl_int *errcodeRet) {
    TRACING_ENTER(clCreateFromGLTexture2D, &context, &flags, &target, &miplevel, &texture, &errcodeRet);

    API_ENTER(errcodeRet);
    DBG_LOG_INPUTS("context", context, "flags", flags, "target", target, "miplevel", miplevel, "texture", texture);
    Context *pContext = nullptr;
    auto returnCode = validateObjects(WithCastToInternal(context, &pContext));

    ErrorCodeHelper err(errcodeRet, returnCode);
    cl_mem image = nullptr;

    if (returnCode != CL_SUCCESS) {
        TRACING_EXIT(clCreateFromGLTexture2D, &image);
        return image;
    }

    if (pContext->getSharing<GLSharingFunctions>() == nullptr) {
        err.set(CL_INVALID_CONTEXT);
        TRACING_EXIT(clCreateFromGLTexture2D, &image);
        return image;
    }

    image = GlTexture::createSharedGlTexture(pContext, flags, target, miplevel, texture, errcodeRet);
    DBG_LOG_INPUTS("image", image);
    TRACING_EXIT(clCreateFromGLTexture2D, &image);
    return image;
}

// deprecated OpenCL 1.1
cl_mem CL_API_CALL clCreateFromGLTexture3D(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel,
                                           cl_GLuint texture, cl_int *errcodeRet) {
    TRACING_ENTER(clCreateFromGLTexture3D, &context, &flags, &target, &miplevel, &texture, &errcodeRet);

    API_ENTER(errcodeRet);
    DBG_LOG_INPUTS("context", context, "flags", flags, "target", target, "miplevel", miplevel, "texture", texture);
    Context *pContext = nullptr;
    auto returnCode = validateObjects(WithCastToInternal(context, &pContext));

    ErrorCodeHelper err(errcodeRet, returnCode);
    cl_mem image = nullptr;

    if (returnCode != CL_SUCCESS) {
        TRACING_EXIT(clCreateFromGLTexture3D, &image);
        return image;
    }

    if (pContext->getSharing<GLSharingFunctions>() == nullptr) {
        err.set(CL_INVALID_CONTEXT);
        TRACING_EXIT(clCreateFromGLTexture3D, &image);
        return image;
    }

    image = GlTexture::createSharedGlTexture(pContext, flags, target, miplevel, texture, errcodeRet);
    DBG_LOG_INPUTS("image", image);
    TRACING_EXIT(clCreateFromGLTexture3D, &image);
    return image;
}

cl_mem CL_API_CALL clCreateFromGLRenderbuffer(cl_context context, cl_mem_flags flags, cl_GLuint renderbuffer, cl_int *errcodeRet) {
    TRACING_ENTER(clCreateFromGLRenderbuffer, &context, &flags, &renderbuffer, &errcodeRet);

    API_ENTER(errcodeRet);
    DBG_LOG_INPUTS("context", context, "flags", flags, "renderbuffer", renderbuffer);
    Context *pContext = nullptr;
    auto returnCode = validateObjects(WithCastToInternal(context, &pContext));

    ErrorCodeHelper err(errcodeRet, returnCode);

    if (returnCode != CL_SUCCESS) {
        cl_mem buffer = nullptr;
        TRACING_EXIT(clCreateFromGLRenderbuffer, &buffer);
        return buffer;
    }

    if (pContext->getSharing<GLSharingFunctions>() == nullptr) {
        err.set(CL_INVALID_CONTEXT);
        cl_mem buffer = nullptr;
        TRACING_EXIT(clCreateFromGLRenderbuffer, &buffer);
        return buffer;
    }

    cl_mem buffer = GlTexture::createSharedGlTexture(pContext, flags, GL_RENDERBUFFER_EXT, 0, renderbuffer, errcodeRet);
    TRACING_EXIT(clCreateFromGLRenderbuffer, &buffer);
    return buffer;
}

cl_int CL_API_CALL clGetGLObjectInfo(cl_mem memobj, cl_gl_object_type *glObjectType, cl_GLuint *glObjectName) {
    TRACING_ENTER(clGetGLObjectInfo, &memobj, &glObjectType, &glObjectName);
    cl_int retValue = CL_SUCCESS;
    API_ENTER(&retValue);
    DBG_LOG_INPUTS("memobj", memobj, "glObjectType", glObjectType, "glObjectName", glObjectName);
    retValue = validateObjects(memobj);
    if (retValue == CL_SUCCESS) {
        auto pMemObj = castToObject<MemObj>(memobj);
        auto handler = (GlSharing *)pMemObj->peekSharingHandler();
        if (handler != nullptr) {
            handler->getGlObjectInfo(glObjectType, glObjectName);
        } else {
            retValue = CL_INVALID_GL_OBJECT;
            TRACING_EXIT(clGetGLObjectInfo, &retValue);
            return retValue;
        }
    }
    TRACING_EXIT(clGetGLObjectInfo, &retValue);
    return retValue;
}

cl_int CL_API_CALL clGetGLTextureInfo(cl_mem memobj, cl_gl_texture_info paramName, size_t paramValueSize, void *paramValue,
                                      size_t *paramValueSizeRet) {
    TRACING_ENTER(clGetGLTextureInfo, &memobj, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    cl_int retValue = CL_SUCCESS;
    API_ENTER(&retValue);
    DBG_LOG_INPUTS("memobj", memobj, "paramName", paramName, "paramValueSize", paramValueSize, "paramValueSize",
                   FileLoggerInstance().infoPointerToString(paramValue, paramValueSize), "paramValueSizeRet",
                   FileLoggerInstance().getInput(paramValueSizeRet, 0));
    retValue = validateObjects(memobj);
    if (retValue == CL_SUCCESS) {
        auto pMemObj = castToObject<MemObj>(memobj);
        auto glTexture = (GlTexture *)pMemObj->peekSharingHandler();
        retValue = glTexture->getGlTextureInfo(paramName, paramValueSize, paramValue, paramValueSizeRet);
    }
    TRACING_EXIT(clGetGLTextureInfo, &retValue);
    return retValue;
}

cl_int CL_API_CALL clEnqueueAcquireGLObjects(cl_command_queue commandQueue, cl_uint numObjects, const cl_mem *memObjects,
                                             cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) {
    TRACING_ENTER(clEnqueueAcquireGLObjects, &commandQueue, &numObjects, &memObjects, &numEventsInWaitList, &eventWaitList, &event);

    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue, "numObjects", numObjects, "memObjects", memObjects, "numEventsInWaitList",
                   numEventsInWaitList, "eventWaitList",
                   FileLoggerInstance().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList), "event",
                   FileLoggerInstance().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));
    CommandQueue *pCommandQueue = nullptr;
    retVal = validateObjects(WithCastToInternal(commandQueue, &pCommandQueue), EventWaitList(numEventsInWaitList, eventWaitList));

    if (retVal == CL_SUCCESS) {
        if (pCommandQueue->getContext().getSharing<GLSharingFunctions>() == nullptr) {
            retVal = CL_INVALID_CONTEXT;
            TRACING_EXIT(clEnqueueAcquireGLObjects, &retVal);
            return retVal;
        }

        for (auto id = 0u; id < numEventsInWaitList; id++) {
            auto event = castToObjectOrAbort<Event>(eventWaitList[id]);
            event->updateExecutionStatus();
            if ((event->peekExecutionStatus() > CL_COMPLETE) && (event->isExternallySynchronized())) {
                if (DebugManager.flags.EnableAsyncEventsHandler.get()) {
                    platform()->getAsyncEventsHandler()->registerEvent(event);
                }
            }
        }
        retVal = pCommandQueue->enqueueAcquireSharedObjects(numObjects, memObjects, numEventsInWaitList, eventWaitList, event,
                                                            CL_COMMAND_ACQUIRE_GL_OBJECTS);
    }

    TRACING_EXIT(clEnqueueAcquireGLObjects, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueReleaseGLObjects(cl_command_queue commandQueue, cl_uint numObjects, const cl_mem *memObjects,
                                             cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) {
    TRACING_ENTER(clEnqueueReleaseGLObjects, &commandQueue, &numObjects, &memObjects, &numEventsInWaitList, &eventWaitList, &event);

    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue, "numObjects", numObjects, "memObjects", memObjects, "numEventsInWaitList",
                   numEventsInWaitList, "eventWaitList",
                   FileLoggerInstance().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList), "event",
                   FileLoggerInstance().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));
    CommandQueue *pCommandQueue = nullptr;
    retVal = validateObjects(WithCastToInternal(commandQueue, &pCommandQueue), EventWaitList(numEventsInWaitList, eventWaitList));

    if (retVal == CL_SUCCESS) {
        if (pCommandQueue->getContext().getSharing<GLSharingFunctions>() == nullptr) {
            retVal = CL_INVALID_CONTEXT;
            TRACING_EXIT(clEnqueueReleaseGLObjects, &retVal);
            return retVal;
        }

        pCommandQueue->finish();
        retVal = pCommandQueue->enqueueReleaseSharedObjects(numObjects, memObjects, numEventsInWaitList, eventWaitList, event,
                                                            CL_COMMAND_RELEASE_GL_OBJECTS);
    }

    TRACING_EXIT(clEnqueueReleaseGLObjects, &retVal);
    return retVal;
}

cl_event CL_API_CALL clCreateEventFromGLsyncKHR(cl_context context, cl_GLsync sync, cl_int *errcodeRet) {
    API_ENTER(errcodeRet);
    DBG_LOG_INPUTS("context", context, "sync", sync);
    Context *pContext = nullptr;
    auto returnCode = validateObjects(WithCastToInternal(context, &pContext));

    ErrorCodeHelper err(errcodeRet, returnCode);

    if (returnCode != CL_SUCCESS) {
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
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("properties", properties, "paramName", paramName, "paramValueSize", paramValueSize, "paramValue",
                   FileLoggerInstance().infoPointerToString(paramValue, paramValueSize), "paramValueSizeRet",
                   FileLoggerInstance().getInput(paramValueSizeRet, 0));
    GetInfoHelper info(paramValue, paramValueSize, paramValueSizeRet);

    uint32_t GLHGLRCHandle = 0;
    uint32_t GLHDCHandle = 0;
    uint32_t propertyType = 0;
    uint32_t propertyValue = 0;

    if (properties != nullptr) {
        while (*properties != 0) {
            propertyType = static_cast<uint32_t>(properties[0]);
            propertyValue = static_cast<uint32_t>(properties[1]);
            properties += 2;
            switch (propertyType) {
            case CL_GL_CONTEXT_KHR:
                GLHGLRCHandle = propertyValue;
                break;
            case CL_WGL_HDC_KHR:
                GLHDCHandle = propertyValue;
                break;
            }
        }
    }

    if ((GLHDCHandle == 0) || (GLHGLRCHandle == 0)) {
        retVal = CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR;
        return retVal;
    }

    auto glSharing = std::make_unique<GLSharingFunctions>();
    glSharing->initGLFunctions();
    if (glSharing->isOpenGlSharingSupported() == false) {
        retVal = CL_INVALID_CONTEXT;
        return retVal;
    }

    if (paramName == CL_DEVICES_FOR_GL_CONTEXT_KHR || paramName == CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR) {
        info.set<cl_device_id>(::platform()->getDevice(0));
        return retVal;
    }

    retVal = CL_INVALID_VALUE;
    return retVal;
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

    Context *pContext = castToObjectOrAbort<Context>(context);
    auto pSharing = pContext->getSharing<GLSharingFunctions>();
    if (!pSharing) {
        return CL_INVALID_CONTEXT;
    }

    return pSharing->getSupportedFormats(flags, imageType, numEntries, glFormats, numTextureFormats);
}
