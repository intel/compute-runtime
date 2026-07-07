/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/sharings/gl/windows/leo_win_enable_gl.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/sharings/gl/leo_cl_gl_api_intel.h"
#include "level_zero/api/opencl/source/sharings/gl/windows/leo_gl_sharing_windows.h"
#include "level_zero/api/opencl/source/sharings/leo_sharing_factory.h"
#include "level_zero/api/opencl/source/sharings/leo_sharing_factory.inl"

#include <memory>

namespace NEO {
namespace LEO {

bool GlSharingContextBuilder::processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue) {
    if (contextData.get() == nullptr) {
        contextData = std::make_unique<GlCreateContextProperties>();
    }

    switch (propertyType) {
    case CL_GL_CONTEXT_KHR:
        contextData->glHGLRCHandle = (GLContext)propertyValue;
        return true;
    case CL_WGL_HDC_KHR:
        contextData->glHDCType = (GLType)CL_WGL_HDC_KHR;
        contextData->glHDCHandle = (GLDisplay)propertyValue;
        return true;
    case CL_GLX_DISPLAY_KHR:
        contextData->glHDCType = (GLType)CL_GLX_DISPLAY_KHR;
        contextData->glHDCHandle = (GLDisplay)propertyValue;
        return true;
    case CL_EGL_DISPLAY_KHR:
        contextData->glHDCType = (GLType)CL_EGL_DISPLAY_KHR;
        contextData->glHDCHandle = (GLDisplay)propertyValue;
        return true;
    }
    return false;
}

bool GlSharingContextBuilder::finalizeProperties(Context &context, int32_t &errcodeRet) {
    if (contextData.get() == nullptr) {
        return true;
    }

    if (contextData->glHGLRCHandle) {
        context.registerSharing(new GLSharingFunctionsWindows(contextData->glHDCType, contextData->glHGLRCHandle,
                                                              nullptr, contextData->glHDCHandle));
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

std::string GlSharingBuilderFactory::getExtensions(DriverInfo *driverInfo) {
    auto isGlSharingEnabled = GLSharingFunctionsWindows::isGlSharingEnabled();

    if (debugManager.flags.AddClGlSharing.get() != -1) {
        isGlSharingEnabled = debugManager.flags.AddClGlSharing.get();
    }

    if (isGlSharingEnabled) {
        return "cl_khr_gl_sharing "
               "cl_khr_gl_depth_images "
               "cl_khr_gl_event "
               "cl_khr_gl_msaa_sharing ";
    }

    return "";
}

void *GlSharingBuilderFactory::getExtensionFunctionAddress(const std::string &functionName) {
    if (debugManager.flags.EnableFormatQuery.get() &&
        functionName == "clGetSupportedGLTextureFormatsINTEL") {
        return ((void *)(clGetSupportedGLTextureFormatsINTEL));
    }

    return nullptr;
}

static SharingFactory::RegisterSharing<GlSharingBuilderFactory, GLSharingFunctionsWindows> glSharing;

} // namespace LEO
} // namespace NEO
