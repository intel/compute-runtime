/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/extensions/public/cl_gl_private_intel.h"
#include "opencl/source/sharings/gl/linux/gl_sharing_linux.h"
#include "opencl/test/unit_test/sharings/gl/gl_dll_helper.h"

#include "config.h"

#include <cstring>

namespace NEO {

namespace glTextureTargets {
static const unsigned int supportedTargets[] = {
    GL_TEXTURE_2D,
};
} // namespace glTextureTargets

class GlSharingFunctionsMock : public GLSharingFunctionsLinux {

    void initMembers();

  public:
    ~GlSharingFunctionsMock() override = default;

    using GLSharingFunctionsLinux::eglGLInteropExportObject;
    using GLSharingFunctionsLinux::eglGLInteropFlushObjects;
    using GLSharingFunctionsLinux::eglGLInteropQueryDeviceInfo;

    using GLSharingFunctionsLinux::glGetIntegerv;
    using GLSharingFunctionsLinux::glGetString;
    using GLSharingFunctionsLinux::glGetStringi;
    using GLSharingFunctionsLinux::isOpenGlExtensionSupported;

    using GLSharingFunctionsLinux::glArbEventMapping;

    using GLSharingFunctionsLinux::getSupportedFormats;
    using GLSharingFunctionsLinux::pfnGlArbSyncObjectCleanup;
    using GLSharingFunctionsLinux::pfnGlArbSyncObjectSetup;
    using GLSharingFunctionsLinux::pfnGlArbSyncObjectSignal;
    using GLSharingFunctionsLinux::pfnGlArbSyncObjectWaitServer;

    GlSharingFunctionsMock(GLType glHDCType, GLContext glHGLRCHandle, GLContext glHGLRCHandleBkpCtx, GLDisplay glHDCHandle)
        : GLSharingFunctionsLinux(glHDCType, glHGLRCHandle, glHGLRCHandleBkpCtx, glHDCHandle) {
        initMembers();
    }
    GlSharingFunctionsMock();

    void setHandles(GLType glHDCType, GLContext glHGLRCHandle, GLContext glHGLRCHandleBkpCtx, GLDisplay glHDCHandle) {
        this->glHDCType = glHDCType;
        this->glHGLRCHandle = glHGLRCHandle;
        this->glHGLRCHandleBkpCtx = glHGLRCHandleBkpCtx;
        this->glHDCHandle = glHDCHandle;
    }

    void setGLInteropQueryDeviceInfoMock(PFNMESAGLINTEROPEGLQUERYDEVICEINFOPROC mock) { eglGLInteropQueryDeviceInfo = mock; }

    void setGLInteropExportObjectMock(PFNMESAGLINTEROPEGLEXPORTOBJECTPROC mock) { eglGLInteropExportObject = mock; }

    void setGLInteropFlushObjectsMock(PFNMESAGLINTEROPEGLFLUSHOBJECTSPROC mock) { eglGLInteropFlushObjects = mock; }
};

class MockGlSharing {
  public:
    MockGlSharing() {}
    MockGlSharing(GLType glHDCType, GLContext glHGLRCHandle, GLContext glHGLRCHandleBkpCtx, GLDisplay glHDCHandle);
    void uploadDataToBufferInfo() {
        dllParam->loadBuffer(bufferInfoOutput);
    }
    void uploadDataToBufferInfo(unsigned int sharedHandle, int bufferOffset, GMM_RESOURCE_INFO *gmmResInfo) {
        bufferInfoOutput.globalShareHandle = sharedHandle;
        bufferInfoOutput.bufferOffset = bufferOffset;
        bufferInfoOutput.pGmmResInfo = gmmResInfo;
        dllParam->loadBuffer(bufferInfoOutput);
    }
    void uploadDataToTextureInfo() {
        dllParam->loadTexture(textureInfoOutput);
    }
    void uploadDataToTextureInfo(unsigned int sharedHandle) {
        textureInfoOutput.globalShareHandle = sharedHandle;
        dllParam->loadTexture(textureInfoOutput);
    }
    void uploadTextureBufferOffsetToTextureInfo(int texBufOffset) {
        textureInfoOutput.textureBufferOffset = texBufOffset;
        dllParam->loadTexture(textureInfoOutput);
    }

    std::unique_ptr<GlSharingFunctionsMock> sharingFunctions = std::make_unique<GlSharingFunctionsMock>();
    std::unique_ptr<GlDllHelper> dllParam = std::make_unique<GlDllHelper>();
    CL_GL_RESOURCE_INFO clGlResourceInfo = {0};
    GL_CL_RESOURCE_INFO glClResourceInfo = {0};
    CL_GL_BUFFER_INFO bufferInfoOutput = {0};
    CL_GL_RESOURCE_INFO textureInfoOutput = {0};
};

class MockGLSharingFunctions : public GLSharingFunctionsLinux {
  public:
    using GLSharingFunctionsLinux::isOpenGlExtensionSupported;

    static void glGetIntegervTest(GLenum pname, GLint *data) {
        if (pname == GL_NUM_EXTENSIONS)
            *data = 2;
    };
    using GLSharingFunctionsLinux::glGetIntegerv;
    using GLSharingFunctionsLinux::glGetString;
    std::unique_ptr<GlDllHelper> dllParam = std::make_unique<GlDllHelper>();
    MockGLSharingFunctions() {
        GLSharingFunctionsLinux::initGLFunctions();
    }
};
} // namespace NEO
