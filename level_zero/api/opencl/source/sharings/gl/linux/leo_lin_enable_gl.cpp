/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/sharings/gl/linux/leo_lin_enable_gl.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/sharings/gl/leo_cl_gl_api_intel.h"
#include "level_zero/api/opencl/source/sharings/gl/linux/leo_gl_sharing_linux.h"
#include "level_zero/api/opencl/source/sharings/leo_sharing_factory.h"
#include "level_zero/api/opencl/source/sharings/leo_sharing_factory.inl"

#include <memory>

namespace NEO {
namespace LEO {

struct GlCreateContextProperties {
    GLType glHDCType = 0;
    GLContext glHGLRCHandle = 0;
    GLDisplay glHDCHandle = 0;
};

GlSharingContextBuilder::GlSharingContextBuilder() = default;
GlSharingContextBuilder::~GlSharingContextBuilder() = default;

bool GlSharingContextBuilder::processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue) {
    if (!contextData) {
        contextData = std::make_unique<GlCreateContextProperties>();
    }

    switch (propertyType) {
    case CL_GL_CONTEXT_KHR:
        contextData->glHGLRCHandle = reinterpret_cast<GLContext>(propertyValue);
        return true;
    case CL_GLX_DISPLAY_KHR:
        contextData->glHDCType = static_cast<GLType>(CL_GLX_DISPLAY_KHR);
        contextData->glHDCHandle = reinterpret_cast<GLDisplay>(propertyValue);
        return true;
    case CL_EGL_DISPLAY_KHR:
        contextData->glHDCType = static_cast<GLType>(CL_EGL_DISPLAY_KHR);
        contextData->glHDCHandle = reinterpret_cast<GLDisplay>(propertyValue);
        return true;
    }
    return false;
}

bool GlSharingContextBuilder::finalizeProperties(Context &context, int32_t &errcodeRet) {
    if (contextData == nullptr) {
        return true;
    }

    if (contextData->glHGLRCHandle) {
        context.registerSharing(new GLSharingFunctionsLinux(contextData->glHDCType, contextData->glHGLRCHandle,
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
    auto isGlSharingEnabled = true;

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
        return reinterpret_cast<void *>(clGetSupportedGLTextureFormatsINTEL);
    }

    return nullptr;
}

static SharingFactory::RegisterSharing<GlSharingBuilderFactory, GLSharingFunctionsLinux> glSharing;

} // namespace LEO
} // namespace NEO
