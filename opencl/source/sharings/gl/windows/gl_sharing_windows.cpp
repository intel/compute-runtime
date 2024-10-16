/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sharings/gl/windows/gl_sharing_windows.h"

#include "opencl/source/context/context.inl"
#include "opencl/source/helpers/gl_helper.h"
#include "opencl/source/sharings/gl/gl_arb_sync_event.h"

namespace Os {
extern const char *openglDllName;
}

namespace NEO {
GLSharingFunctionsWindows::GLSharingFunctionsWindows(GLType glhdcType, GLContext glhglrcHandle, GLContext glhglrcHandleBkpCtx, GLDisplay glhdcHandle)
    : glHDCType(glhdcType), glHGLRCHandle(glhglrcHandle), glHGLRCHandleBkpCtx(glhglrcHandleBkpCtx), glHDCHandle(glhdcHandle) {
    GLSharingFunctionsWindows::initGLFunctions();
    updateOpenGLContext();
    createBackupContext();
}
GLSharingFunctionsWindows::~GLSharingFunctionsWindows() {
    if (pfnWglDeleteContext) {
        pfnWglDeleteContext(glHGLRCHandleBkpCtx);
    }
}

bool GLSharingFunctionsWindows::isGlSharingEnabled() {
    static bool oglLibAvailable = std::unique_ptr<OsLibrary>(OsLibrary::loadFunc({Os::openglDllName})).get() != nullptr;
    return oglLibAvailable;
}

void GLSharingFunctionsWindows::createBackupContext() {
    if (pfnWglCreateContext) {
        glHGLRCHandleBkpCtx = pfnWglCreateContext(glHDCHandle);
        pfnWglShareLists(glHGLRCHandle, glHGLRCHandleBkpCtx);
    }
}

GLboolean GLSharingFunctionsWindows::setSharedOCLContextState() {
    ContextInfo ctxInfo = {0};
    GLboolean retVal = glSetSharedOCLContextState(glHDCHandle, glHGLRCHandle, CL_TRUE, &ctxInfo);
    if (retVal == GL_FALSE) {
        return GL_FALSE;
    }
    glContextHandle = ctxInfo.contextHandle;
    glDeviceHandle = ctxInfo.deviceHandle;

    return retVal;
}

bool GLSharingFunctionsWindows::isOpenGlExtensionSupported(const unsigned char *pExtensionString) {
    bool loadedNull = (glGetStringi == nullptr) || (glGetIntegerv == nullptr);
    if (loadedNull) {
        return false;
    }

    cl_int numberOfExtensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &numberOfExtensions);
    for (cl_int i = 0; i < numberOfExtensions; i++) {
        std::basic_string<unsigned char> pString = glGetStringi(GL_EXTENSIONS, i);
        if (pString == pExtensionString) {
            return true;
        }
    }
    return false;
}

bool GLSharingFunctionsWindows::isOpenGlSharingSupported() {

    std::basic_string<unsigned char> vendor = glGetString(GL_VENDOR);
    const unsigned char intelVendor[] = "Intel";

    if ((vendor.empty()) || (vendor != intelVendor)) {
        return false;
    }
    std::basic_string<unsigned char> version = glGetString(GL_VERSION);
    if (version.empty()) {
        return false;
    }

    bool isOpenGLES = false;
    const unsigned char versionES[] = "OpenGL ES";
    if (version.find(versionES) != std::string::npos) {
        isOpenGLES = true;
    }

    if (isOpenGLES == true) {
        const unsigned char versionES1[] = "OpenGL ES 1.";
        if (version.find(versionES1) != std::string::npos) {
            const unsigned char supportGLOES[] = "GL_OES_framebuffer_object";
            if (isOpenGlExtensionSupported(supportGLOES) == false) {
                return false;
            }
        }
    } else {
        if (version[0] < '3') {
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
    glLibrary.reset(OsLibrary::loadFunc({Os::openglDllName}));

    if (glLibrary->isLoaded()) {
        GlFunctionHelper wglLibrary(glLibrary.get(), "wglGetProcAddress");
        glGetCurrentContext = (*glLibrary)["wglGetCurrentContext"];
        glGetCurrentDisplay = (*glLibrary)["wglGetCurrentDC"];
        glGetString = (*glLibrary)["glGetString"];
        glGetIntegerv = (*glLibrary)["glGetIntegerv"];
        pfnWglCreateContext = (*glLibrary)["wglCreateContext"];
        pfnWglDeleteContext = (*glLibrary)["wglDeleteContext"];
        pfnWglShareLists = (*glLibrary)["wglShareLists"];
        wglMakeCurrent = (*glLibrary)["wglMakeCurrent"];

        glSetSharedOCLContextState = wglLibrary["wglSetSharedOCLContextStateINTEL"];
        glAcquireSharedBuffer = wglLibrary["wglAcquireSharedBufferINTEL"];
        glReleaseSharedBuffer = wglLibrary["wglReleaseSharedBufferINTEL"];
        glAcquireSharedRenderBuffer = wglLibrary["wglAcquireSharedRenderBufferINTEL"];
        glReleaseSharedRenderBuffer = wglLibrary["wglReleaseSharedRenderBufferINTEL"];
        glAcquireSharedTexture = wglLibrary["wglAcquireSharedTextureINTEL"];
        glReleaseSharedTexture = wglLibrary["wglReleaseSharedTextureINTEL"];
        glRetainSync = wglLibrary["wglRetainSyncINTEL"];
        glReleaseSync = wglLibrary["wglReleaseSyncINTEL"];
        glGetSynciv = wglLibrary["wglGetSyncivINTEL"];
        glGetStringi = wglLibrary["glGetStringi"];
        glGetLuid = wglLibrary["wglGetLuidINTEL"];
    }
    this->pfnGlArbSyncObjectCleanup = cleanupArbSyncObject;
    this->pfnGlArbSyncObjectSetup = setupArbSyncObject;
    this->pfnGlArbSyncObjectSignal = signalArbSyncObject;
    this->pfnGlArbSyncObjectWaitServer = serverWaitForArbSyncObject;

    return 1;
}

LUID GLSharingFunctionsWindows::getAdapterLuid(GLContext glhglrcHandle) const {
    if (glGetLuid) {
        return glGetLuid(glhglrcHandle);
    }
    return {};
}

template GLSharingFunctionsWindows *Context::getSharing<GLSharingFunctionsWindows>();

} // namespace NEO
