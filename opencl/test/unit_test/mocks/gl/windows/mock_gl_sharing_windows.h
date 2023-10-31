/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/extensions/public/cl_gl_private_intel.h"
#include "opencl/source/sharings/gl/windows/gl_sharing_windows.h"
#include "opencl/test/unit_test/sharings/gl/gl_dll_helper.h"

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

class GlSharingFunctionsMock : public GLSharingFunctionsWindows {

    void initMembers();

  public:
    static GLboolean OSAPI mockGLSetSharedOCLContextState(GLDisplay, GLContext, GLboolean, GLvoid *pBufferInfo) {
        GLSetSharedOCLContextStateCalled++;
        return (GLboolean)1;
    };
    ~GlSharingFunctionsMock() override = default;

    using GLSharingFunctionsWindows::glAcquireSharedBuffer;
    using GLSharingFunctionsWindows::glAcquireSharedRenderBuffer;
    using GLSharingFunctionsWindows::glAcquireSharedTexture;
    using GLSharingFunctionsWindows::glGetCurrentContext;
    using GLSharingFunctionsWindows::glGetCurrentDisplay;
    using GLSharingFunctionsWindows::glGetIntegerv;
    using GLSharingFunctionsWindows::glGetString;
    using GLSharingFunctionsWindows::glGetStringi;
    using GLSharingFunctionsWindows::glGetSynciv;
    using GLSharingFunctionsWindows::glReleaseSharedBuffer;
    using GLSharingFunctionsWindows::glReleaseSharedRenderBuffer;
    using GLSharingFunctionsWindows::glReleaseSharedTexture;
    using GLSharingFunctionsWindows::glReleaseSync;
    using GLSharingFunctionsWindows::glRetainSync;
    using GLSharingFunctionsWindows::glSetSharedOCLContextState;
    using GLSharingFunctionsWindows::isOpenGlExtensionSupported;
    using GLSharingFunctionsWindows::pfnWglCreateContext;
    using GLSharingFunctionsWindows::pfnWglDeleteContext;
    using GLSharingFunctionsWindows::pfnWglShareLists;
    using GLSharingFunctionsWindows::setSharedOCLContextState;
    using GLSharingFunctionsWindows::wglMakeCurrent;

    using GLSharingFunctionsWindows::glArbEventMapping;
    using GLSharingFunctionsWindows::glContextHandle;
    using GLSharingFunctionsWindows::glDeviceHandle;

    using GLSharingFunctionsWindows::getSupportedFormats;
    using GLSharingFunctionsWindows::pfnGlArbSyncObjectCleanup;
    using GLSharingFunctionsWindows::pfnGlArbSyncObjectSetup;
    using GLSharingFunctionsWindows::pfnGlArbSyncObjectSignal;
    using GLSharingFunctionsWindows::pfnGlArbSyncObjectWaitServer;

    GlSharingFunctionsMock(GLType glHDCType, GLContext glHGLRCHandle, GLContext glHGLRCHandleBkpCtx, GLDisplay glHDCHandle)
        : GLSharingFunctionsWindows(glHDCType, glHGLRCHandle, glHGLRCHandleBkpCtx, glHDCHandle) {
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

    void setGLAcquireSharedTextureMock(PFNOGLAcquireSharedTextureINTEL mock) { glAcquireSharedTexture = mock; }
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
    std::unique_ptr<GlDllHelper> dllParam = std::make_unique<GlDllHelper>();
    CL_GL_RESOURCE_INFO clGlResourceInfo = {0};
    GL_CL_RESOURCE_INFO glClResourceInfo = {0};
    CL_GL_BUFFER_INFO bufferInfoOutput = {0};
    CL_GL_RESOURCE_INFO textureInfoOutput = {0};
    GLMockReturnedValues glMockReturnedValues = {0};
};

class MockGLSharingFunctions : public GLSharingFunctionsWindows {
  public:
    using GLSharingFunctionsWindows::isOpenGlExtensionSupported;
    using GLSharingFunctionsWindows::setSharedOCLContextState;

    static void OSAPI glGetIntegervTest(GLenum pname, GLint *data) {
        if (pname == GL_NUM_EXTENSIONS)
            *data = 2;
    };
    using GLSharingFunctionsWindows::glGetIntegerv;
    using GLSharingFunctionsWindows::glGetString;
    std::unique_ptr<GlDllHelper> dllParam = std::make_unique<GlDllHelper>();
    MockGLSharingFunctions() {
        GLSharingFunctionsWindows::initGLFunctions();
    }
};
} // namespace NEO
