/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "config.h"
#include <cstring>
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/sharings/gl/gl_sharing.h"
#include "public/cl_gl_private_intel.h"

namespace OCLRT {
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
extern int GLAcquireSharedBufferCalled;
extern int GLAcquireSharedRenderBufferCalled;
extern int GLAcquireSharedTextureCalled;
extern int GLReleaseSharedBufferCalled;
extern int GLReleaseSharedRenderBufferCalled;
extern int GLReleaseSharedTextureCalled;
extern int GLGetCurrentContextCalled;
extern int GLGetCurrentDisplayCalled;
extern int GLMakeCurrentCalled;
extern int GLDeleteContextCalled;
extern int WGLCreateContextCalled;
extern int WGLShareListsCalled;
extern int WGLDeleteContextCalled;
extern int EGLCreateContextCalled;
extern int EGLDeleteContextCalled;
extern int EGLChooseConfigCalled;
extern int GlxChooseFBConfigCalled;
extern int GlxQueryContextCalled;
extern int GlxCreateNewContextCalled;
extern int GlxDeleteContextCalled;
extern int GlxIsDirectCalled;
extern int GLRetainSyncCalled;
extern int GLReleaseSyncCalled;
extern int GLGetSyncivCalled;
extern CL_GL_BUFFER_INFO bufferInfoInput;
extern CL_GL_BUFFER_INFO bufferInfoOutput;
extern CL_GL_RESOURCE_INFO textureInfoInput;
extern CL_GL_RESOURCE_INFO textureInfoOutput;
extern EGLBkpContextParams eglBkpContextParams;
extern GLXBkpContextParams glxBkpContextParams;
extern GLMockReturnedValues glMockReturnedValues;

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

    static GLboolean OSAPI mockGLSetSharedOCLContextState(GLDisplay, GLContext, GLboolean, GLvoid *pBufferInfo) {
        GLSetSharedOCLContextStateCalled++;
        return (GLboolean)1;
    };
    static GLboolean OSAPI mockGLAcquireSharedBuffer(GLDisplay, GLContext, GLContext, GLvoid *pResourceInfo) {
        auto pBufferInfo = (CL_GL_BUFFER_INFO *)pResourceInfo;
        bufferInfoInput = *pBufferInfo;
        pBufferInfo->bufferSize = bufferInfoOutput.bufferSize;
        pBufferInfo->globalShareHandle = bufferInfoOutput.globalShareHandle;
        pBufferInfo->pGmmResInfo = bufferInfoOutput.pGmmResInfo;
        pBufferInfo->bufferOffset = bufferInfoOutput.bufferOffset;
        GLAcquireSharedBufferCalled++;
        return (GLboolean)1;
    };
    static GLboolean OSAPI mockGLAcquireSharedRenderBuffer(GLDisplay, GLContext, GLContext, GLvoid *pResourceInfo) {
        auto pTextureInfo = (CL_GL_RESOURCE_INFO *)pResourceInfo;
        textureInfoInput = *pTextureInfo;
        pTextureInfo->globalShareHandle = textureInfoOutput.globalShareHandle;
        pTextureInfo->pGmmResInfo = textureInfoOutput.pGmmResInfo;
        pTextureInfo->glInternalFormat = GL_RGBA8;
        GLAcquireSharedRenderBufferCalled++;
        return (GLboolean)1;
    };
    static GLboolean OSAPI mockGLAcquireSharedTexture(GLDisplay, GLContext, GLContext, GLvoid *pResourceInfo) {
        auto pTextureInfo = (CL_GL_RESOURCE_INFO *)pResourceInfo;
        textureInfoInput = *pTextureInfo;
        pTextureInfo->globalShareHandle = textureInfoOutput.globalShareHandle;
        pTextureInfo->globalShareHandleMCS = textureInfoOutput.globalShareHandleMCS;
        if (pTextureInfo->target == GL_TEXTURE_BUFFER) {
            // size and width for texture buffer are queried from textureInfo - not from gmm
            pTextureInfo->textureBufferSize = textureInfoOutput.textureBufferSize;
            pTextureInfo->textureBufferWidth = textureInfoOutput.textureBufferWidth;
        }
        pTextureInfo->pGmmResInfo = textureInfoOutput.pGmmResInfo;
        pTextureInfo->glInternalFormat = textureInfoOutput.glInternalFormat ? textureInfoOutput.glInternalFormat : GL_RGBA8;
        pTextureInfo->glHWFormat = textureInfoOutput.glHWFormat;
        pTextureInfo->textureBufferOffset = textureInfoOutput.textureBufferOffset;
        pTextureInfo->numberOfSamples = textureInfoOutput.numberOfSamples;
        GLAcquireSharedTextureCalled++;
        return (GLboolean)1;
    };
    static GLboolean OSAPI mockGLReleaseSharedTexture(GLDisplay, GLContext, GLContext, GLvoid *pResourceInfo) {
        textureInfoInput = *static_cast<CL_GL_RESOURCE_INFO *>(pResourceInfo);
        GLReleaseSharedTextureCalled++;
        return (GLboolean)1;
    };
    static GLboolean OSAPI mockGLReleaseSharedRenderBuffer(GLDisplay, GLContext, GLContext, GLvoid *pResourceInfo) {
        textureInfoInput = *static_cast<CL_GL_RESOURCE_INFO *>(pResourceInfo);
        GLReleaseSharedRenderBufferCalled++;
        return (GLboolean)1;
    };
    static GLboolean OSAPI mockGLReleaseSharedBuffer(GLDisplay, GLContext, GLContext, GLvoid *pResourceInfo) {
        bufferInfoInput = *static_cast<CL_GL_BUFFER_INFO *>(pResourceInfo);
        GLReleaseSharedBufferCalled++;
        return (GLboolean)1;
    };
    static GLContext OSAPI mockGLGetCurrentContext() {
        GLGetCurrentContextCalled++;
        return glMockReturnedValues.currentContext;
    };
    static GLDisplay OSAPI mockGLGetCurrentDisplay() {
        GLGetCurrentDisplayCalled++;
        return glMockReturnedValues.currentDisplay;
    };
    static BOOL OSAPI mockWGLMakeCurrent(HDC, HGLRC context) {
        GLMakeCurrentCalled++;
        glMockReturnedValues.madeCurrentContext = context;
        if (glMockReturnedValues.forceMakeCurrentCallFail) {
            if (glMockReturnedValues.failsCounter < glMockReturnedValues.numberOfCallFails) {
                glMockReturnedValues.failsCounter++;
                return GL_FALSE;
            }
        }
        return (GLboolean)1;
    };

    static GLContext OSAPI mockWGlCreateContext(GLDisplay hdcHandle) {
        WGLCreateContextCalled++;
        return (GLContext)0x101;
    }
    static int OSAPI mockWGlShareLists(GLContext contextHandle, GLContext backupContextHandle) {
        WGLShareListsCalled++;
        return 1;
    }

    static BOOL OSAPI mockWGLDeleteContext(HGLRC context) {
        WGLDeleteContextCalled++;
        GLDeleteContextCalled++;
        return (GLboolean)1;
    }

    static GLboolean OSAPI mockGlRetainSync(GLDisplay HDCHandle, GLContext ContextHandle, GLContext BackupContextHandle,
                                            GLvoid *pSyncInfo) {
        GLRetainSyncCalled++;
        GL_CL_SYNC_INFO *syncInfo = (GL_CL_SYNC_INFO *)(pSyncInfo);
        syncInfo->pSync = (void *)0x123;
        return GL_TRUE;
    }
    static GLboolean OSAPI mockGlReleaseSync(GLDisplay HDCHandle, GLContext ContextHandle, GLContext BackupContextHandle,
                                             GLvoid *pSync) {
        GLReleaseSyncCalled++;
        return GL_TRUE;
    }
    static void OSAPI mockGlGetSynciv(GLvoid *pSync, GLenum pname, GLint *value) {
        GLGetSyncivCalled++;
        *value = glMockReturnedValues.syncivRetVal;
    }
    void initMembers();

  public:
    ~GlSharingFunctionsMock() override = default;

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
    void uploadDataToBufferInfo() { bufferInfoOutput = m_bufferInfoOutput; }
    void uploadDataToBufferInfo(unsigned int sharedHandle, int bufferOffset) {
        m_bufferInfoOutput.globalShareHandle = sharedHandle;
        m_bufferInfoOutput.bufferOffset = bufferOffset;
        bufferInfoOutput = m_bufferInfoOutput;
    }
    void uploadDataToTextureInfo() { textureInfoOutput = m_textureInfoOutput; }
    void uploadDataToTextureInfo(unsigned int sharedHandle) {
        m_textureInfoOutput.globalShareHandle = sharedHandle;
        textureInfoOutput = m_textureInfoOutput;
    }
    void uploadTextureBufferOffsetToTextureInfo(int texBufOffset) {
        m_textureInfoOutput.textureBufferOffset = texBufOffset;
        textureInfoOutput = m_textureInfoOutput;
    }
    void overrideGetCurrentValues(GLContext ctx, GLDisplay display, bool forceMakeCurrentFail = false, int numberOfFails = 0) {
        glMockReturnedValues.currentContext = ctx;
        glMockReturnedValues.currentDisplay = display;
        glMockReturnedValues.forceMakeCurrentCallFail = forceMakeCurrentFail;
        glMockReturnedValues.numberOfCallFails = numberOfFails;
        glMockReturnedValues.failsCounter = 0;
    }
    void setGetSyncivReturnValue(int val) { glMockReturnedValues.syncivRetVal = val; }

    GlSharingFunctionsMock m_sharingFunctions;

    CL_GL_RESOURCE_INFO m_clGlResourceInfo = {0};
    GL_CL_RESOURCE_INFO m_glClResourceInfo = {0};
    CL_GL_BUFFER_INFO m_bufferInfoOutput = {0};
    CL_GL_RESOURCE_INFO m_textureInfoOutput = {0};
};

class MockGLSharingFunctions : public GLSharingFunctions {
  public:
    static const char *arrayStringi[2];
    static const char *arrayString[2];
    static GLboolean GLSetSharedOCLContextStateReturnedValue;
    static bool SharingEnabled;
    static const GLubyte *OSAPI glGetStringTest(GLenum name) {
        if (name == GL_VENDOR)
            return (const GLubyte *)arrayString[0];
        if (name == GL_VERSION)
            return (const GLubyte *)arrayString[1];
        return nullptr;
    };
    static const GLubyte *OSAPI glGetStringiTest(GLenum name, GLuint index) { return (const GLubyte *)arrayStringi[index]; };
    static void OSAPI glGetIntegervTest(GLenum pname, GLint *data) {
        if (pname == GL_NUM_EXTENSIONS)
            *data = 2;
    };
    static GLboolean OSAPI GLSetSharedOCLContextStateTest(GLDisplay HDCHandle, GLContext ContextHandle, GLboolean State,
                                                          GLvoid *pContextInfo) {
        ((ContextInfo *)pContextInfo)->ContextHandle = 1;
        ((ContextInfo *)pContextInfo)->DeviceHandle = 2;
        return GLSetSharedOCLContextStateReturnedValue;
    }

    void *loadGlFunction(const char *FunctionName, DWORD HDC) { return GLSharingFunctions::loadGlFunction(FunctionName, HDC); };

    void setGetStringFcn(PFNglGetString fcn) { glGetString = fcn; }

    void setglGetIntegervToNull() { glGetIntegerv = nullptr; }
    MockGLSharingFunctions() {
        glGetString = (PFNglGetString)glGetStringTest;
        glGetStringi = (PFNglGetStringi)glGetStringiTest;
        glGetIntegerv = (PFNglGetIntegerv)glGetIntegervTest;
        GLSetSharedOCLContextState = (PFNOGLSetSharedOCLContextStateINTEL)GLSetSharedOCLContextStateTest;
        arrayStringi[0] = "GL_OES_framebuffer_object";
        arrayStringi[1] = "GL_EXT_framebuffer_object";
        arrayString[0] = "Intel";
        arrayString[1] = "4.0";
        GLSetSharedOCLContextStateReturnedValue = 1;
        MockGLSharingFunctions::SharingEnabled = 1;
    }
};
} // namespace OCLRT
