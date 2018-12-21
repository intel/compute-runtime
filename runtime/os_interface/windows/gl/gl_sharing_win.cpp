/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <cstdint>
#include <memory>
#include "runtime/helpers/timestamp_packet.h"
#include "runtime/os_interface/windows/windows_wrapper.h"
#include "runtime/sharings/gl/gl_arb_sync_event.h"
#include "runtime/sharings/gl/gl_sharing.h"

namespace Os {
extern const char *openglDllName;
}

namespace OCLRT {

GLboolean GLSharingFunctions::makeCurrent(GLContext contextHandle, GLDisplay displayHandle) {
    if (displayHandle == 0) {
        displayHandle = GLHDCHandle;
    }
    return this->wglMakeCurrent(displayHandle, contextHandle);
}

GLSharingFunctions::~GLSharingFunctions() {
    if (pfnWglDeleteContext) {
        pfnWglDeleteContext(GLHGLRCHandleBkpCtx);
    }
}

GLboolean GLSharingFunctions::initGLFunctions() {
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

bool GLSharingFunctions::isGlSharingEnabled() {
    static bool oglLibAvailable = std::unique_ptr<OsLibrary>(OsLibrary::load(Os::openglDllName)).get() != nullptr;
    return oglLibAvailable;
}

bool GLSharingFunctions::isOpenGlExtensionSupported(const char *pExtensionString) {
    bool LoadedNull = (glGetStringi == nullptr) || (glGetIntegerv == nullptr);
    if (LoadedNull) {
        return false;
    }

    cl_int NumberOfExtensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &NumberOfExtensions);
    for (cl_int i = 0; i < NumberOfExtensions; i++) {
        const char *pString = reinterpret_cast<const char *>(glGetStringi(GL_EXTENSIONS, i));
        if (strcmp(pString, pExtensionString) == 0) {
            return true;
        }
    }
    return false;
}

bool GLSharingFunctions::isOpenGlSharingSupported() {

    const char *Vendor = reinterpret_cast<const char *>(glGetString(GL_VENDOR));
    if ((Vendor == NULL) || (strcmp(Vendor, "Intel") != 0)) {
        return false;
    }

    const char *Version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
    if (Version == NULL) {
        return false;
    }

    bool IsOpenGLES = false;
    if (strstr(Version, "OpenGL ES") != NULL) {
        IsOpenGLES = true;
    }

    if (IsOpenGLES == true) {
        if (strstr(Version, "OpenGL ES 1.") != NULL) {
            if (isOpenGlExtensionSupported("GL_OES_framebuffer_object") == false) {
                return false;
            }
        }
    } else {
        if (Version[0] < '3') {
            if (isOpenGlExtensionSupported("GL_EXT_framebuffer_object") == false) {
                return false;
            }
        }
    }

    return true;
}

void GLSharingFunctions::createBackupContext() {
    if (pfnWglCreateContext) {
        GLHGLRCHandleBkpCtx = pfnWglCreateContext(GLHDCHandle);
        pfnWglShareLists(GLHGLRCHandle, GLHGLRCHandleBkpCtx);
    }
}
} // namespace OCLRT
