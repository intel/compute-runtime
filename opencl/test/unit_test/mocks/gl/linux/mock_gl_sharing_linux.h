/*
 * Copyright (C) 2020-2023 Intel Corporation
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

    using GLSharingFunctionsLinux::eglMakeCurrent;
    using GLSharingFunctionsLinux::glAcquireSharedBuffer;
    using GLSharingFunctionsLinux::glAcquireSharedRenderBuffer;
    using GLSharingFunctionsLinux::glAcquireSharedTexture;
    using GLSharingFunctionsLinux::glGetCurrentContext;
    using GLSharingFunctionsLinux::glGetCurrentDisplay;
    using GLSharingFunctionsLinux::glGetIntegerv;
    using GLSharingFunctionsLinux::glGetString;
    using GLSharingFunctionsLinux::glGetStringi;
    using GLSharingFunctionsLinux::glGetSynciv;
    using GLSharingFunctionsLinux::glReleaseSharedBuffer;
    using GLSharingFunctionsLinux::glReleaseSharedRenderBuffer;
    using GLSharingFunctionsLinux::glReleaseSharedTexture;
    using GLSharingFunctionsLinux::glReleaseSync;
    using GLSharingFunctionsLinux::glRetainSync;
    using GLSharingFunctionsLinux::glSetSharedOCLContextState;
    using GLSharingFunctionsLinux::isOpenGlExtensionSupported;
    using GLSharingFunctionsLinux::pfnEglCreateContext;
    using GLSharingFunctionsLinux::pfnEglDeleteContext;
    using GLSharingFunctionsLinux::pfnEglShareLists;
    using GLSharingFunctionsLinux::setSharedOCLContextState;

    using GLSharingFunctionsLinux::glArbEventMapping;
    using GLSharingFunctionsLinux::glContextHandle;
    using GLSharingFunctionsLinux::glDeviceHandle;

    using GLSharingFunctionsLinux::getSupportedFormats;
    using GLSharingFunctionsLinux::pfnGlArbSyncObjectCleanup;
    using GLSharingFunctionsLinux::pfnGlArbSyncObjectSetup;
    using GLSharingFunctionsLinux::pfnGlArbSyncObjectSignal;
    using GLSharingFunctionsLinux::pfnGlArbSyncObjectWaitServer;

    GlSharingFunctionsMock(GLType glHDCType, GLContext glHGLRCHandle, GLContext glHGLRCHandleBkpCtx, GLDisplay glHDCHandle)
        : GLSharingFunctionsLinux(glHDCType, glHGLRCHandle, glHGLRCHandleBkpCtx, glHDCHandle) {
        initMembers();
        updateOpenGLContext();
        createBackupContext();
    }
    GlSharingFunctionsMock();

    void setHandles(GLType glHDCType, GLContext glHGLRCHandle, GLContext glHGLRCHandleBkpCtx, GLDisplay glHDCHandle) {
        this->glHDCType = glHDCType;
        this->glHGLRCHandle = glHGLRCHandle;
        this->glHGLRCHandleBkpCtx = glHGLRCHandleBkpCtx;
        this->glHDCHandle = glHDCHandle;
    }

    void setGLAcquireSharedBufferMock(PFNOGLAcquireSharedBufferINTEL mock) { glAcquireSharedBuffer = mock; }

    void setGLAcquireSharedTextureMock(PFNEGLEXPORTDMABUFIMAGEMESAPROC mock) { glAcquireSharedTexture = mock; }
};

class MockGlSharing {
  public:
    MockGlSharing() {}
    MockGlSharing(GLType glHDCType, GLContext glHGLRCHandle, GLContext glHGLRCHandleBkpCtx, GLDisplay glHDCHandle);
    void uploadDataToBufferInfo() {
        dllParam->loadBuffer(m_bufferInfoOutput);
    }
    void uploadDataToBufferInfo(unsigned int sharedHandle, int bufferOffset, GMM_RESOURCE_INFO *gmmResInfo) {
        m_bufferInfoOutput.globalShareHandle = sharedHandle;
        m_bufferInfoOutput.bufferOffset = bufferOffset;
        m_bufferInfoOutput.pGmmResInfo = gmmResInfo;
        dllParam->loadBuffer(m_bufferInfoOutput);
    }
    void uploadDataToTextureInfo() {
        dllParam->loadTexture(m_textureInfoOutput);
    }
    void uploadDataToTextureInfo(unsigned int sharedHandle) {
        m_textureInfoOutput.globalShareHandle = sharedHandle;
        dllParam->loadTexture(m_textureInfoOutput);
    }
    void uploadTextureBufferOffsetToTextureInfo(int texBufOffset) {
        m_textureInfoOutput.textureBufferOffset = texBufOffset;
        dllParam->loadTexture(m_textureInfoOutput);
    }

    std::unique_ptr<GlSharingFunctionsMock> sharingFunctions = std::make_unique<GlSharingFunctionsMock>();
    std::unique_ptr<GlDllHelper> dllParam = std::make_unique<GlDllHelper>();
    CL_GL_RESOURCE_INFO m_clGlResourceInfo = {0};
    GL_CL_RESOURCE_INFO m_glClResourceInfo = {0};
    CL_GL_BUFFER_INFO m_bufferInfoOutput = {0};
    CL_GL_RESOURCE_INFO m_textureInfoOutput = {0};
};

class MockGLSharingFunctions : public GLSharingFunctionsLinux {
  public:
    using GLSharingFunctionsLinux::isOpenGlExtensionSupported;
    using GLSharingFunctionsLinux::setSharedOCLContextState;

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
