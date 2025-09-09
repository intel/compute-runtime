/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/os_interface/os_interface.h"

#include "opencl/source/api/api.h"
#include "opencl/source/api/api_enter.h"
#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/source/event/async_events_handler.h"
#include "opencl/source/helpers/base_object.h"
#include "opencl/source/helpers/cl_validators.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/mem_obj/mem_obj.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/sharings/gl/gl_buffer.h"
#include "opencl/source/sharings/gl/gl_sync_event.h"
#include "opencl/source/sharings/gl/gl_texture.h"
#include "opencl/source/tracing/tracing_notify.h"
#include "opencl/source/utilities/cl_logger.h"

#include "CL/cl.h"
#include "CL/cl_gl.h"
#include "config.h"

using namespace NEO;

cl_mem CL_API_CALL clCreateFromGLBuffer(cl_context context, cl_mem_flags flags, cl_GLuint bufobj, cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateFromGlBuffer, &context, &flags, &bufobj, &errcodeRet);

    API_ENTER(errcodeRet);
    DBG_LOG_INPUTS("context", context, "flags", flags, "bufobj", bufobj);

    Context *pContext = nullptr;

    auto returnCode = validateObjects(withCastToInternal(context, &pContext));

    ErrorCodeHelper err(errcodeRet, returnCode);

    if (returnCode != CL_SUCCESS) {
        cl_mem buffer = nullptr;
        TRACING_EXIT(ClCreateFromGlBuffer, &buffer);
        return buffer;
    }

    if (pContext->getSharing<GLSharingFunctions>() == nullptr) {
        err.set(CL_INVALID_CONTEXT);
        cl_mem buffer = nullptr;
        TRACING_EXIT(ClCreateFromGlBuffer, &buffer);
        return buffer;
    }

    cl_mem buffer = GlBuffer::createSharedGlBuffer(pContext, flags, bufobj, errcodeRet);
    TRACING_EXIT(ClCreateFromGlBuffer, &buffer);
    return buffer;
}

cl_mem CL_API_CALL clCreateFromGLTexture(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel,
                                         cl_GLuint texture, cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateFromGlTexture, &context, &flags, &target, &miplevel, &texture, &errcodeRet);

    API_ENTER(errcodeRet);
    DBG_LOG_INPUTS("context", context, "flags", flags, "target", target, "miplevel", miplevel, "texture", texture);
    Context *pContext = nullptr;

    auto returnCode = validateObjects(withCastToInternal(context, &pContext));

    ErrorCodeHelper err(errcodeRet, returnCode);
    cl_mem image = nullptr;

    if (returnCode != CL_SUCCESS) {
        TRACING_EXIT(ClCreateFromGlTexture, &image);
        return image;
    }

    if (pContext->getSharing<GLSharingFunctions>() == nullptr) {
        err.set(CL_INVALID_CONTEXT);
        TRACING_EXIT(ClCreateFromGlTexture, &image);
        return image;
    }
    image = GlTexture::createSharedGlTexture(pContext, flags, target, miplevel, texture, errcodeRet);
    DBG_LOG_INPUTS("image", image);
    TRACING_EXIT(ClCreateFromGlTexture, &image);
    return image;
}

// deprecated OpenCL 1.1
cl_mem CL_API_CALL clCreateFromGLTexture2D(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel,
                                           cl_GLuint texture, cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateFromGlTexture2D, &context, &flags, &target, &miplevel, &texture, &errcodeRet);

    API_ENTER(errcodeRet);
    DBG_LOG_INPUTS("context", context, "flags", flags, "target", target, "miplevel", miplevel, "texture", texture);
    Context *pContext = nullptr;
    auto returnCode = validateObjects(withCastToInternal(context, &pContext));

    ErrorCodeHelper err(errcodeRet, returnCode);
    cl_mem image = nullptr;

    if (returnCode != CL_SUCCESS) {
        TRACING_EXIT(ClCreateFromGlTexture2D, &image);
        return image;
    }

    if (pContext->getSharing<GLSharingFunctions>() == nullptr) {
        err.set(CL_INVALID_CONTEXT);
        TRACING_EXIT(ClCreateFromGlTexture2D, &image);
        return image;
    }

    image = GlTexture::createSharedGlTexture(pContext, flags, target, miplevel, texture, errcodeRet);
    DBG_LOG_INPUTS("image", image);
    TRACING_EXIT(ClCreateFromGlTexture2D, &image);
    return image;
}

// deprecated OpenCL 1.1
cl_mem CL_API_CALL clCreateFromGLTexture3D(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel,
                                           cl_GLuint texture, cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateFromGlTexture3D, &context, &flags, &target, &miplevel, &texture, &errcodeRet);

    API_ENTER(errcodeRet);
    DBG_LOG_INPUTS("context", context, "flags", flags, "target", target, "miplevel", miplevel, "texture", texture);
    Context *pContext = nullptr;
    auto returnCode = validateObjects(withCastToInternal(context, &pContext));

    ErrorCodeHelper err(errcodeRet, returnCode);
    cl_mem image = nullptr;

    if (returnCode != CL_SUCCESS) {
        TRACING_EXIT(ClCreateFromGlTexture3D, &image);
        return image;
    }

    if (pContext->getSharing<GLSharingFunctions>() == nullptr) {
        err.set(CL_INVALID_CONTEXT);
        TRACING_EXIT(ClCreateFromGlTexture3D, &image);
        return image;
    }

    image = GlTexture::createSharedGlTexture(pContext, flags, target, miplevel, texture, errcodeRet);
    DBG_LOG_INPUTS("image", image);
    TRACING_EXIT(ClCreateFromGlTexture3D, &image);
    return image;
}

cl_mem CL_API_CALL clCreateFromGLRenderbuffer(cl_context context, cl_mem_flags flags, cl_GLuint renderbuffer, cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateFromGlRenderbuffer, &context, &flags, &renderbuffer, &errcodeRet);

    API_ENTER(errcodeRet);
    DBG_LOG_INPUTS("context", context, "flags", flags, "renderbuffer", renderbuffer);
    Context *pContext = nullptr;
    auto returnCode = validateObjects(withCastToInternal(context, &pContext));

    ErrorCodeHelper err(errcodeRet, returnCode);

    if (returnCode != CL_SUCCESS) {
        cl_mem buffer = nullptr;
        TRACING_EXIT(ClCreateFromGlRenderbuffer, &buffer);
        return buffer;
    }

    if (pContext->getSharing<GLSharingFunctions>() == nullptr) {
        err.set(CL_INVALID_CONTEXT);
        cl_mem buffer = nullptr;
        TRACING_EXIT(ClCreateFromGlRenderbuffer, &buffer);
        return buffer;
    }

    cl_mem buffer = GlTexture::createSharedGlTexture(pContext, flags, GL_RENDERBUFFER_EXT, 0, renderbuffer, errcodeRet);
    TRACING_EXIT(ClCreateFromGlRenderbuffer, &buffer);
    return buffer;
}

cl_int CL_API_CALL clGetGLObjectInfo(cl_mem memobj, cl_gl_object_type *glObjectType, cl_GLuint *glObjectName) {
    TRACING_ENTER(ClGetGlObjectInfo, &memobj, &glObjectType, &glObjectName);
    cl_int retValue = CL_SUCCESS;
    API_ENTER(&retValue);
    DBG_LOG_INPUTS("memobj", memobj, "glObjectType", glObjectType, "glObjectName", glObjectName);
    retValue = validateObjects(memobj);
    if (retValue == CL_SUCCESS) {
        auto pMemObj = castToObject<MemObj>(memobj);
        auto handler = static_cast<GlSharing *>(pMemObj->peekSharingHandler());
        if (handler != nullptr) {
            handler->getGlObjectInfo(glObjectType, glObjectName);
        } else {
            retValue = CL_INVALID_GL_OBJECT;
            TRACING_EXIT(ClGetGlObjectInfo, &retValue);
            return retValue;
        }
    }
    TRACING_EXIT(ClGetGlObjectInfo, &retValue);
    return retValue;
}

cl_int CL_API_CALL clGetGLTextureInfo(cl_mem memobj, cl_gl_texture_info paramName, size_t paramValueSize, void *paramValue,
                                      size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetGlTextureInfo, &memobj, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    cl_int retValue = CL_SUCCESS;
    API_ENTER(&retValue);
    DBG_LOG_INPUTS("memobj", memobj, "paramName", paramName, "paramValueSize", paramValueSize, "paramValueSize",
                   fileLoggerInstance().infoPointerToString(paramValue, paramValueSize), "paramValueSizeRet",
                   fileLoggerInstance().getInput(paramValueSizeRet, 0));
    retValue = validateObjects(memobj);
    if (retValue == CL_SUCCESS) {
        auto pMemObj = castToObject<MemObj>(memobj);
        auto glTexture = static_cast<GlTexture *>(pMemObj->peekSharingHandler());
        retValue = glTexture->getGlTextureInfo(paramName, paramValueSize, paramValue, paramValueSizeRet);
    }
    TRACING_EXIT(ClGetGlTextureInfo, &retValue);
    return retValue;
}

cl_int CL_API_CALL clEnqueueAcquireGLObjects(cl_command_queue commandQueue, cl_uint numObjects, const cl_mem *memObjects,
                                             cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) {
    TRACING_ENTER(ClEnqueueAcquireGlObjects, &commandQueue, &numObjects, &memObjects, &numEventsInWaitList, &eventWaitList, &event);

    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue, "numObjects", numObjects, "memObjects", memObjects, "numEventsInWaitList",
                   numEventsInWaitList, "eventWaitList",
                   getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList), "event",
                   getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));
    CommandQueue *pCommandQueue = nullptr;
    retVal = validateObjects(withCastToInternal(commandQueue, &pCommandQueue), EventWaitList(numEventsInWaitList, eventWaitList));

    if (retVal == CL_SUCCESS) {
        if (pCommandQueue->getContext().getSharing<GLSharingFunctions>() == nullptr) {
            retVal = CL_INVALID_CONTEXT;
            TRACING_EXIT(ClEnqueueAcquireGlObjects, &retVal);
            return retVal;
        }

        for (auto id = 0u; id < numEventsInWaitList; id++) {
            auto event = castToObjectOrAbort<Event>(eventWaitList[id]);
            event->updateExecutionStatus();
            if ((event->peekExecutionStatus() > CL_COMPLETE) && (event->isExternallySynchronized())) {
                if (debugManager.flags.EnableAsyncEventsHandler.get()) {
                    event->getContext()->getAsyncEventsHandler().registerEvent(event);
                }
            }
        }
        retVal = pCommandQueue->enqueueAcquireSharedObjects(numObjects, memObjects, numEventsInWaitList, eventWaitList, event,
                                                            CL_COMMAND_ACQUIRE_GL_OBJECTS);
    }

    TRACING_EXIT(ClEnqueueAcquireGlObjects, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueReleaseGLObjects(cl_command_queue commandQueue, cl_uint numObjects, const cl_mem *memObjects,
                                             cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) {
    TRACING_ENTER(ClEnqueueReleaseGlObjects, &commandQueue, &numObjects, &memObjects, &numEventsInWaitList, &eventWaitList, &event);

    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue, "numObjects", numObjects, "memObjects", memObjects, "numEventsInWaitList",
                   numEventsInWaitList, "eventWaitList",
                   getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList), "event",
                   getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));
    CommandQueue *pCommandQueue = nullptr;
    retVal = validateObjects(withCastToInternal(commandQueue, &pCommandQueue), EventWaitList(numEventsInWaitList, eventWaitList));

    if (retVal == CL_SUCCESS) {
        if (pCommandQueue->getContext().getSharing<GLSharingFunctions>() == nullptr) {
            retVal = CL_INVALID_CONTEXT;
            TRACING_EXIT(ClEnqueueReleaseGlObjects, &retVal);
            return retVal;
        }
        pCommandQueue->finish(true);

        retVal = pCommandQueue->enqueueReleaseSharedObjects(numObjects, memObjects, numEventsInWaitList, eventWaitList, event,
                                                            CL_COMMAND_RELEASE_GL_OBJECTS);
    }

    TRACING_EXIT(ClEnqueueReleaseGlObjects, &retVal);
    return retVal;
}

cl_event CL_API_CALL clCreateEventFromGLsyncKHR(cl_context context, cl_GLsync sync, cl_int *errcodeRet) {
    API_ENTER(errcodeRet);
    DBG_LOG_INPUTS("context", context, "sync", sync);
    Context *pContext = nullptr;
    auto returnCode = validateObjects(withCastToInternal(context, &pContext));

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
                   fileLoggerInstance().infoPointerToString(paramValue, paramValueSize), "paramValueSizeRet",
                   fileLoggerInstance().getInput(paramValueSizeRet, 0));
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
        retVal = CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR;
        return retVal;
    }

    glSharing->initGLFunctions();
    if (glSharing->isOpenGlSharingSupported() == false) {
        retVal = CL_INVALID_CONTEXT;
        return retVal;
    }

    if (paramName == CL_DEVICES_FOR_GL_CONTEXT_KHR || paramName == CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR) {
        if (!platform) {
            platform = (*platformsImpl)[0].get();
        }

        ClDevice *deviceToReturn = nullptr;
        for (auto i = 0u; i < platform->getNumDevices(); i++) {
            auto device = platform->getClDevice(i);
            if (glSharing->isHandleCompatible(*device->getRootDeviceEnvironment().osInterface->getDriverModel(), glHglrcHandle)) {
                deviceToReturn = device;
                break;
            }
        }
        if (!deviceToReturn) {
            retVal = CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR;
            return retVal;
        }
        info.set<cl_device_id>(deviceToReturn);
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
