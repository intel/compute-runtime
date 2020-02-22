/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sharings/gl/windows/gl_sharing_windows.h"

#include "opencl/source/context/context.inl"
#include "opencl/source/helpers/windows/gl_helper.h"
#include "opencl/source/sharings/gl/gl_arb_sync_event.h"

namespace NEO {
GLSharingFunctionsWindows::GLSharingFunctionsWindows(GLType glhdcType, GLContext glhglrcHandle, GLContext glhglrcHandleBkpCtx, GLDisplay glhdcHandle)
    : GLHDCType(glhdcType), GLHGLRCHandle(glhglrcHandle), GLHGLRCHandleBkpCtx(glhglrcHandleBkpCtx), GLHDCHandle(glhdcHandle) {
    initGLFunctions();
    updateOpenGLContext();
    createBackupContext();
}
GLSharingFunctionsWindows::~GLSharingFunctionsWindows() {
    if (pfnWglDeleteContext) {
        pfnWglDeleteContext(GLHGLRCHandleBkpCtx);
    }
}

bool GLSharingFunctionsWindows::isGlSharingEnabled() {
    static bool oglLibAvailable = std::unique_ptr<OsLibrary>(OsLibrary::load(Os::openglDllName)).get() != nullptr;
    return oglLibAvailable;
}

void GLSharingFunctionsWindows::createBackupContext() {
    if (pfnWglCreateContext) {
        GLHGLRCHandleBkpCtx = pfnWglCreateContext(GLHDCHandle);
        pfnWglShareLists(GLHGLRCHandle, GLHGLRCHandleBkpCtx);
    }
}

GLboolean GLSharingFunctionsWindows::setSharedOCLContextState() {
    ContextInfo CtxInfo = {0};
    GLboolean retVal = GLSetSharedOCLContextState(GLHDCHandle, GLHGLRCHandle, CL_TRUE, &CtxInfo);
    if (retVal == GL_FALSE) {
        return GL_FALSE;
    }
    GLContextHandle = CtxInfo.ContextHandle;
    GLDeviceHandle = CtxInfo.DeviceHandle;

    return retVal;
}

bool GLSharingFunctionsWindows::isOpenGlExtensionSupported(const unsigned char *pExtensionString) {
    bool LoadedNull = (glGetStringi == nullptr) || (glGetIntegerv == nullptr);
    if (LoadedNull) {
        return false;
    }

    cl_int NumberOfExtensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &NumberOfExtensions);
    for (cl_int i = 0; i < NumberOfExtensions; i++) {
        std::basic_string<unsigned char> pString = glGetStringi(GL_EXTENSIONS, i);
        if (pString == pExtensionString) {
            return true;
        }
    }
    return false;
}

bool GLSharingFunctionsWindows::isOpenGlSharingSupported() {

    std::basic_string<unsigned char> Vendor = glGetString(GL_VENDOR);
    const unsigned char intelVendor[] = "Intel";

    if ((Vendor.empty()) || (Vendor != intelVendor)) {
        return false;
    }
    std::basic_string<unsigned char> Version = glGetString(GL_VERSION);
    if (Version.empty()) {
        return false;
    }

    bool IsOpenGLES = false;
    const unsigned char versionES[] = "OpenGL ES";
    if (Version.find(versionES) != std::string::npos) {
        IsOpenGLES = true;
    }

    if (IsOpenGLES == true) {
        const unsigned char versionES1[] = "OpenGL ES 1.";
        if (Version.find(versionES1) != std::string::npos) {
            const unsigned char supportGLOES[] = "GL_OES_framebuffer_object";
            if (isOpenGlExtensionSupported(supportGLOES) == false) {
                return false;
            }
        }
    } else {
        if (Version[0] < '3') {
            const unsigned char supportGLEXT[] = "GL_EXT_framebuffer_object";
            if (isOpenGlExtensionSupported(supportGLEXT) == false) {
                return false;
            }
        }
    }

    return true;
}

GlArbSyncEvent *GLSharingFunctionsWindows::getGlArbSyncEvent(Event &baseEvent) {
    std::lock_guard<std::mutex> lock{glArbEventMutex};
    auto it = glArbEventMapping.find(&baseEvent);
    if (it != glArbEventMapping.end()) {
        return it->second;
    }
    return nullptr;
}

void GLSharingFunctionsWindows::removeGlArbSyncEventMapping(Event &baseEvent) {
    std::lock_guard<std::mutex> lock{glArbEventMutex};
    auto it = glArbEventMapping.find(&baseEvent);
    if (it == glArbEventMapping.end()) {
        DEBUG_BREAK_IF(it == glArbEventMapping.end());
        return;
    }
    glArbEventMapping.erase(it);
}

GLboolean GLSharingFunctionsWindows::initGLFunctions() {
    glLibrary.reset(OsLibrary::load(Os::openglDllName));

    if (glLibrary->isLoaded()) {
        glFunctionHelper wglLibrary(glLibrary.get(), "wglGetProcAddress");
        GLGetCurrentContext = (*glLibrary)["wglGetCurrentContext"];
        GLGetCurrentDisplay = (*glLibrary)["wglGetCurrentDC"];
        glGetString = (*glLibrary)["glGetString"];
        glGetIntegerv = (*glLibrary)["glGetIntegerv"];
        pfnWglCreateContext = (*glLibrary)["wglCreateContext"];
        pfnWglDeleteContext = (*glLibrary)["wglDeleteContext"];
        pfnWglShareLists = (*glLibrary)["wglShareLists"];
        wglMakeCurrent = (*glLibrary)["wglMakeCurrent"];

        GLSetSharedOCLContextState = wglLibrary["wglSetSharedOCLContextStateINTEL"];
        GLAcquireSharedBuffer = wglLibrary["wglAcquireSharedBufferINTEL"];
        GLReleaseSharedBuffer = wglLibrary["wglReleaseSharedBufferINTEL"];
        GLAcquireSharedRenderBuffer = wglLibrary["wglAcquireSharedRenderBufferINTEL"];
        GLReleaseSharedRenderBuffer = wglLibrary["wglReleaseSharedRenderBufferINTEL"];
        GLAcquireSharedTexture = wglLibrary["wglAcquireSharedTextureINTEL"];
        GLReleaseSharedTexture = wglLibrary["wglReleaseSharedTextureINTEL"];
        GLRetainSync = wglLibrary["wglRetainSyncINTEL"];
        GLReleaseSync = wglLibrary["wglReleaseSyncINTEL"];
        GLGetSynciv = wglLibrary["wglGetSyncivINTEL"];
        glGetStringi = wglLibrary["glGetStringi"];
    }
    this->pfnGlArbSyncObjectCleanup = cleanupArbSyncObject;
    this->pfnGlArbSyncObjectSetup = setupArbSyncObject;
    this->pfnGlArbSyncObjectSignal = signalArbSyncObject;
    this->pfnGlArbSyncObjectWaitServer = serverWaitForArbSyncObject;

    return 1;
}

template GLSharingFunctionsWindows *Context::getSharing<GLSharingFunctionsWindows>();

} // namespace NEO
