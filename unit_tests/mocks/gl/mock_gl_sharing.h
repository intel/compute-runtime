/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "public/cl_gl_private_intel.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/sharings/gl/gl_sharing.h"
#include "unit_tests/os_interface/windows/gl/gl_dll_helper.h"

#include "config.h"

#include <cstring>

namespace NEO {
struct EGLBkpContextParams {
    int32_t configAttrs;
    int32_t contextAttrs[3];
    int32_t numConfigs;
};

struct GLXBkpContextParams {
    int FBConfigAttrs;
    int queryAttribute;
    int renderType;
};

struct GLMockReturnedValues {
    GLContext currentContext;
    GLDisplay currentDisplay;
    GLContext madeCurrentContext;
    bool forceMakeCurrentCallFail;
    int numberOfCallFails;
    int failsCounter;
    int syncivRetVal;
};

extern int GLSetSharedOCLContextStateCalled;
extern int EGLCreateContextCalled;
extern int EGLDeleteContextCalled;
extern int EGLChooseConfigCalled;
extern int GlxChooseFBConfigCalled;
extern int GlxQueryContextCalled;
extern int GlxCreateNewContextCalled;
extern int GlxDeleteContextCalled;
extern int GlxIsDirectCalled;
extern EGLBkpContextParams eglBkpContextParams;
extern GLXBkpContextParams glxBkpContextParams;

namespace glTextureTargets {
static const unsigned int supportedTargets[] = {
    GL_TEXTURE_1D,
    GL_TEXTURE_1D_ARRAY,
    GL_TEXTURE_BUFFER,
    GL_TEXTURE_2D,
    GL_TEXTURE_2D_ARRAY,
    GL_TEXTURE_RECTANGLE,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
    GL_TEXTURE_CUBE_MAP_POSITIVE_X,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
    GL_TEXTURE_3D,
    GL_RENDERBUFFER_EXT,
    GL_TEXTURE_2D_MULTISAMPLE,
    GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
};
}

class GlSharingFunctionsMock : public GLSharingFunctions {

    void initMembers();

  public:
    static GLboolean OSAPI mockGLSetSharedOCLContextState(GLDisplay, GLContext, GLboolean, GLvoid *pBufferInfo) {
        GLSetSharedOCLContextStateCalled++;
        return (GLboolean)1;
    };
    ~GlSharingFunctionsMock() override = default;

    using GLSharingFunctions::GLAcquireSharedBuffer;
    using GLSharingFunctions::GLAcquireSharedRenderBuffer;
    using GLSharingFunctions::GLAcquireSharedTexture;
    using GLSharingFunctions::GLGetCurrentContext;
    using GLSharingFunctions::GLGetCurrentDisplay;
    using GLSharingFunctions::glGetIntegerv;
    using GLSharingFunctions::glGetString;
    using GLSharingFunctions::glGetStringi;
    using GLSharingFunctions::GLGetSynciv;
    using GLSharingFunctions::GLReleaseSharedBuffer;
    using GLSharingFunctions::GLReleaseSharedRenderBuffer;
    using GLSharingFunctions::GLReleaseSharedTexture;
    using GLSharingFunctions::GLReleaseSync;
    using GLSharingFunctions::GLRetainSync;
    using GLSharingFunctions::GLSetSharedOCLContextState;
    using GLSharingFunctions::pfnWglCreateContext;
    using GLSharingFunctions::pfnWglDeleteContext;
    using GLSharingFunctions::pfnWglShareLists;
    using GLSharingFunctions::wglMakeCurrent;

    using GLSharingFunctions::glArbEventMapping;
    using GLSharingFunctions::GLContextHandle;
    using GLSharingFunctions::GLDeviceHandle;

    using GLSharingFunctions::pfnGlArbSyncObjectCleanup;
    using GLSharingFunctions::pfnGlArbSyncObjectSetup;
    using GLSharingFunctions::pfnGlArbSyncObjectSignal;
    using GLSharingFunctions::pfnGlArbSyncObjectWaitServer;

    GlSharingFunctionsMock(GLType GLHDCType, GLContext GLHGLRCHandle, GLContext GLHGLRCHandleBkpCtx, GLDisplay GLHDCHandle)
        : GLSharingFunctions(GLHDCType, GLHGLRCHandle, GLHGLRCHandleBkpCtx, GLHDCHandle) {
        initMembers();
        updateOpenGLContext();
        createBackupContext();
    }
    GlSharingFunctionsMock();

    void setHandles(GLType GLHDCType, GLContext GLHGLRCHandle, GLContext GLHGLRCHandleBkpCtx, GLDisplay GLHDCHandle) {
        this->GLHDCType = GLHDCType;
        this->GLHGLRCHandle = GLHGLRCHandle;
        this->GLHGLRCHandleBkpCtx = GLHGLRCHandleBkpCtx;
        this->GLHDCHandle = GLHDCHandle;
    }

    void setGLAcquireSharedBufferMock(PFNOGLAcquireSharedBufferINTEL mock) { GLAcquireSharedBuffer = mock; }

    void setGLAcquireSharedTextureMock(PFNOGLAcquireSharedTextureINTEL mock) { GLAcquireSharedTexture = mock; }
};

class MockGlSharing {
  public:
    MockGlSharing() {}
    MockGlSharing(GLType GLHDCType, GLContext GLHGLRCHandle, GLContext GLHGLRCHandleBkpCtx, GLDisplay GLHDCHandle);
    void uploadDataToBufferInfo() {
        dllParam->loadBuffer(m_bufferInfoOutput);
    }
    void uploadDataToBufferInfo(unsigned int sharedHandle, int bufferOffset) {
        m_bufferInfoOutput.globalShareHandle = sharedHandle;
        m_bufferInfoOutput.bufferOffset = bufferOffset;
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
    void overrideGetCurrentValues(GLContext ctx, GLDisplay display, bool forceMakeCurrentFail = false, int numberOfFails = 0) {
        glMockReturnedValues.currentContext = ctx;
        glMockReturnedValues.currentDisplay = display;
        glMockReturnedValues.forceMakeCurrentCallFail = forceMakeCurrentFail;
        glMockReturnedValues.numberOfCallFails = numberOfFails;
        glMockReturnedValues.failsCounter = 0;
        dllParam->setGlMockReturnedValues(glMockReturnedValues);
    }
    void setGetSyncivReturnValue(int val) {
        glMockReturnedValues.syncivRetVal = val;
        dllParam->setGlMockReturnedValues(glMockReturnedValues);
    }

    std::unique_ptr<GlSharingFunctionsMock> sharingFunctions = std::make_unique<GlSharingFunctionsMock>();
    std::unique_ptr<glDllHelper> dllParam = std::make_unique<glDllHelper>();
    CL_GL_RESOURCE_INFO m_clGlResourceInfo = {0};
    GL_CL_RESOURCE_INFO m_glClResourceInfo = {0};
    CL_GL_BUFFER_INFO m_bufferInfoOutput = {0};
    CL_GL_RESOURCE_INFO m_textureInfoOutput = {0};
    GLMockReturnedValues glMockReturnedValues = {0};
};

class MockGLSharingFunctions : public GLSharingFunctions {
  public:
    static bool SharingEnabled;

    static void OSAPI glGetIntegervTest(GLenum pname, GLint *data) {
        if (pname == GL_NUM_EXTENSIONS)
            *data = 2;
    };
    using GLSharingFunctions::glGetIntegerv;
    using GLSharingFunctions::glGetString;
    std::unique_ptr<glDllHelper> dllParam = std::make_unique<glDllHelper>();
    MockGLSharingFunctions() {
        GLSharingFunctions::initGLFunctions();
        MockGLSharingFunctions::SharingEnabled = 1;
    }
};
} // namespace NEO
