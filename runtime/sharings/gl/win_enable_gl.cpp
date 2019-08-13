/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/context/context.h"
#include "runtime/context/context.inl"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/sharings/gl/cl_gl_api_intel.h"
#include "runtime/sharings/gl/enable_gl.h"
#include "runtime/sharings/gl/gl_sharing.h"
#include "runtime/sharings/sharing_factory.h"
#include "runtime/sharings/sharing_factory.inl"

#include <memory>

namespace NEO {

bool GlSharingContextBuilder::processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue,
                                                cl_int &errcodeRet) {
    if (contextData.get() == nullptr) {
        contextData = std::make_unique<GlCreateContextProperties>();
    }
    bool res = false;

    switch (propertyType) {
    case CL_GL_CONTEXT_KHR:
        contextData->GLHGLRCHandle = (GLContext)propertyValue;
        res = true;
        break;
    case CL_WGL_HDC_KHR:
        contextData->GLHDCType = (GLType)CL_WGL_HDC_KHR;
        contextData->GLHDCHandle = (GLDisplay)propertyValue;
        res = true;
        break;
    case CL_GLX_DISPLAY_KHR:
        contextData->GLHDCType = (GLType)CL_GLX_DISPLAY_KHR;
        contextData->GLHDCHandle = (GLDisplay)propertyValue;
        res = true;
        break;
    case CL_EGL_DISPLAY_KHR:
        contextData->GLHDCType = (GLType)CL_EGL_DISPLAY_KHR;
        contextData->GLHDCHandle = (GLDisplay)propertyValue;
        res = true;
        break;
    case CL_CGL_SHAREGROUP_KHR:
        errcodeRet = CL_INVALID_PROPERTY;
        res = true;
        break;
    }
    return res;
}

bool GlSharingContextBuilder::finalizeProperties(Context &context, int32_t &errcodeRet) {
    if (contextData.get() == nullptr)
        return true;

    if (contextData->GLHGLRCHandle) {
        context.registerSharing(
            new GLSharingFunctions(contextData->GLHDCType, contextData->GLHGLRCHandle, nullptr, contextData->GLHDCHandle));
    }

    contextData.reset(nullptr);
    return true;
}

std::unique_ptr<SharingContextBuilder> GlSharingBuilderFactory::createContextBuilder() {
    return std::make_unique<GlSharingContextBuilder>();
};

void GlSharingBuilderFactory::fillGlobalDispatchTable() {
    icdGlobalDispatchTable.clCreateFromGLBuffer = clCreateFromGLBuffer;
    icdGlobalDispatchTable.clCreateFromGLTexture = clCreateFromGLTexture;
    icdGlobalDispatchTable.clCreateFromGLTexture2D = clCreateFromGLTexture2D;
    icdGlobalDispatchTable.clCreateFromGLTexture3D = clCreateFromGLTexture3D;
    icdGlobalDispatchTable.clCreateFromGLRenderbuffer = clCreateFromGLRenderbuffer;
    icdGlobalDispatchTable.clGetGLObjectInfo = clGetGLObjectInfo;
    icdGlobalDispatchTable.clGetGLTextureInfo = clGetGLTextureInfo;
    icdGlobalDispatchTable.clEnqueueAcquireGLObjects = clEnqueueAcquireGLObjects;
    icdGlobalDispatchTable.clEnqueueReleaseGLObjects = clEnqueueReleaseGLObjects;
    icdGlobalDispatchTable.clCreateEventFromGLsyncKHR = clCreateEventFromGLsyncKHR;
    icdGlobalDispatchTable.clGetGLContextInfoKHR = clGetGLContextInfoKHR;
}

std::string GlSharingBuilderFactory::getExtensions() {
    if (DebugManager.flags.AddClGlSharing.get()) {
        return "cl_khr_gl_sharing "
               "cl_khr_gl_depth_images "
               "cl_khr_gl_event "
               "cl_khr_gl_msaa_sharing ";
    } else if (GLSharingFunctions::isGlSharingEnabled()) {
        return "cl_khr_gl_sharing "
               "cl_khr_gl_depth_images "
               "cl_khr_gl_event "
               "cl_khr_gl_msaa_sharing ";
    }
    return "";
}

void *GlSharingBuilderFactory::getExtensionFunctionAddress(const std::string &functionName) {
    if (DebugManager.flags.EnableFormatQuery.get() &&
        functionName == "clGetSupportedGLTextureFormatsINTEL") {
        return ((void *)(clGetSupportedGLTextureFormatsINTEL));
    }

    return nullptr;
}

static SharingFactory::RegisterSharing<GlSharingBuilderFactory, GLSharingFunctions> glSharing;
} // namespace NEO
