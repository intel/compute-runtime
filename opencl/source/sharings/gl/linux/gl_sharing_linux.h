/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/extensions/public/cl_gl_private_intel.h"
#include "opencl/source/sharings/gl/gl_sharing.h"
#include "opencl/source/sharings/gl/linux/include/gl_types.h"

#include <GL/gl.h>

#include <EGL/eglext.h>

namespace NEO {
// OpenGL API names
typedef GLboolean (*PFNOGLSetSharedOCLContextStateINTEL)(GLDisplay hdcHandle, GLContext contextHandle, GLboolean state, GLvoid *pContextInfo);
typedef GLboolean (*PFNOGLAcquireSharedBufferINTEL)(GLDisplay hdcHandle, GLContext contextHandle, GLContext backupContextHandle, GLvoid *pBufferInfo);
typedef GLboolean (*PFNOGLAcquireSharedRenderBufferINTEL)(GLDisplay hdcHandle, GLContext contextHandle, GLContext backupContextHandle, GLvoid *pResourceInfo);
typedef GLboolean (*PFNOGLReleaseSharedBufferINTEL)(GLDisplay hdcHandle, GLContext contextHandle, GLContext backupContextHandle, GLvoid *pBufferInfo);
typedef GLboolean (*PFNOGLReleaseSharedRenderBufferINTEL)(GLDisplay hdcHandle, GLContext contextHandle, GLContext backupContextHandle, GLvoid *pResourceInfo);
typedef GLboolean (*PFNOGLReleaseSharedTextureINTEL)(GLDisplay hdcHandle, GLContext contextHandle, GLContext backupContextHandle, GLvoid *pResourceInfo);
typedef GLboolean (*PFNOGLRetainSyncINTEL)(GLDisplay hdcHandle, GLContext contextHandle, GLContext backupContextHandle, GLvoid *pSyncInfo);
typedef GLboolean (*PFNOGLReleaseSyncINTEL)(GLDisplay hdcHandle, GLContext contextHandle, GLContext backupContextHandle, GLvoid *pSync);
typedef void (*PFNOGLGetSyncivINTEL)(GLvoid *pSync, GLenum pname, GLint *value);

typedef const GLubyte *(*PFNglGetString)(GLenum name);
typedef const GLubyte *(*PFNglGetStringi)(GLenum name, GLuint index);
typedef void (*PFNglGetIntegerv)(GLenum pname, GLint *params);
typedef void (*PFNglBindTexture)(GLenum target, GLuint texture);
typedef void (*PFNglGetTexLevelParameteriv)(GLenum target, GLint level, GLenum pname, GLint *params);

typedef bool (*PFNglArbSyncObjectSetup)(GLSharingFunctions &sharing, OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo);
typedef void (*PFNglArbSyncObjectCleanup)(OSInterface &osInterface, CL_GL_SYNC_INFO *glSyncInfo);
typedef void (*PFNglArbSyncObjectSignal)(OsContext &osContext, CL_GL_SYNC_INFO &glSyncInfo);
typedef void (*PFNglArbSyncObjectWaitServer)(OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo);

typedef EGLImage (*PFNeglCreateImage)(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLAttrib *attribList);
typedef EGLBoolean (*PFNeglDestroyImage)(EGLDisplay dpy, EGLImage image);

class GLSharingFunctionsLinux : public GLSharingFunctions {
  public:
    GLSharingFunctionsLinux() = default;
    GLSharingFunctionsLinux(GLType glhdcType, GLContext glhglrcHandle, GLContext glhglrcHandleBkpCtx, GLDisplay glhdcHandle);
    ~GLSharingFunctionsLinux() override;

    OS_HANDLE getGLDeviceHandle() const { return glDeviceHandle; }
    OS_HANDLE getGLContextHandle() const { return glContextHandle; }

    GLboolean initGLFunctions() override;
    bool isOpenGlSharingSupported() override;
    static bool isGlSharingEnabled();

    // Arb sync event
    template <typename EventType = GlArbSyncEvent>
    auto getOrCreateGlArbSyncEvent(Event &baseEvent) -> decltype(EventType::create(baseEvent));
    GlArbSyncEvent *getGlArbSyncEvent(Event &baseEvent);
    void removeGlArbSyncEventMapping(Event &baseEvent);

    // Gl functions
    GLboolean acquireSharedBufferINTEL(GLvoid *pBufferInfo) {
        return glAcquireSharedBuffer(glHDCHandle, glHGLRCHandle, glHGLRCHandleBkpCtx, pBufferInfo);
    }
    GLboolean releaseSharedBufferINTEL(GLvoid *pBufferInfo) {
        return glReleaseSharedBuffer(glHDCHandle, glHGLRCHandle, glHGLRCHandleBkpCtx, pBufferInfo);
    }
    GLboolean acquireSharedRenderBuffer(GLvoid *pResourceInfo) {
        return glAcquireSharedRenderBuffer(glHDCHandle, glHGLRCHandle, glHGLRCHandleBkpCtx, pResourceInfo);
    }
    GLboolean releaseSharedRenderBuffer(GLvoid *pResourceInfo) {
        return glReleaseSharedRenderBuffer(glHDCHandle, glHGLRCHandle, glHGLRCHandleBkpCtx, pResourceInfo);
    }
    EGLBoolean acquireSharedTexture(CL_GL_RESOURCE_INFO *pResourceInfo) {
        int fds;
        int stride, offset;
        int miplevel = 0;

        EGLAttrib attribList[] = {EGL_GL_TEXTURE_LEVEL, miplevel, EGL_NONE};
        EGLImage image = eglCreateImage(glHDCHandle, glHGLRCHandle, EGL_GL_TEXTURE_2D, reinterpret_cast<EGLClientBuffer>(static_cast<uintptr_t>(pResourceInfo->name)), &attribList[0]);
        if (image == EGL_NO_IMAGE) {
            return EGL_FALSE;
        }

        EGLBoolean ret = glAcquireSharedTexture(glHDCHandle, image, &fds, &stride, &offset);
        if (ret == EGL_TRUE && fds > 0) {
            pResourceInfo->globalShareHandle = fds;
        } else {
            eglDestroyImage(glHDCHandle, image);
            ret = EGL_FALSE;
        }

        return ret;
    }
    GLboolean releaseSharedTexture(GLvoid *pResourceInfo) {
        return 1;
    }
    GLboolean retainSync(GLvoid *pSyncInfo) {
        return glRetainSync(glHDCHandle, glHGLRCHandle, glHGLRCHandleBkpCtx, pSyncInfo);
    }
    GLboolean releaseSync(GLvoid *pSync) {
        return glReleaseSync(glHDCHandle, glHGLRCHandle, glHGLRCHandleBkpCtx, pSync);
    }
    void getSynciv(GLvoid *pSync, GLenum pname, GLint *value) {
        return glGetSynciv(pSync, pname, value);
    }
    GLContext getBackupContextHandle() {
        return glHGLRCHandleBkpCtx;
    }
    GLContext getContextHandle() {
        return glHGLRCHandle;
    }
    bool glArbSyncObjectSetup(OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo) {
        return pfnGlArbSyncObjectSetup(*this, osInterface, glSyncInfo);
    }
    void glArbSyncObjectCleanup(OSInterface &osInterface, CL_GL_SYNC_INFO *glSyncInfo) {
        pfnGlArbSyncObjectCleanup(osInterface, glSyncInfo);
    }
    void glArbSyncObjectSignal(OsContext &osContext, CL_GL_SYNC_INFO &glSyncInfo) {
        pfnGlArbSyncObjectSignal(osContext, glSyncInfo);
    }
    void glArbSyncObjectWaitServer(OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo) {
        pfnGlArbSyncObjectWaitServer(osInterface, glSyncInfo);
    }
    // Buffer reuse
    std::mutex mutex;
    std::vector<std::pair<unsigned int, GraphicsAllocation *>> graphicsAllocationsForGlBufferReuse;

    PFNglGetTexLevelParameteriv glGetTexLevelParameteriv = nullptr;

  protected:
    GLboolean setSharedOCLContextState();
    bool isOpenGlExtensionSupported(const unsigned char *pExtentionString);

    // Handles
    GLType glHDCType = 0;
    GLContext glHGLRCHandle = 0;
    GLContext glHGLRCHandleBkpCtx = 0;
    GLDisplay glHDCHandle = 0;
    OS_HANDLE glDeviceHandle = 0;
    OS_HANDLE glContextHandle = 0;

    // GL functions
    std::unique_ptr<OsLibrary> glLibrary;
    std::unique_ptr<OsLibrary> eglLibrary;
    PFNOGLSetSharedOCLContextStateINTEL glSetSharedOCLContextState = nullptr;
    PFNOGLAcquireSharedBufferINTEL glAcquireSharedBuffer = nullptr;
    PFNOGLReleaseSharedBufferINTEL glReleaseSharedBuffer = nullptr;
    PFNOGLAcquireSharedRenderBufferINTEL glAcquireSharedRenderBuffer = nullptr;
    PFNOGLReleaseSharedRenderBufferINTEL glReleaseSharedRenderBuffer = nullptr;
    PFNEGLEXPORTDMABUFIMAGEMESAPROC glAcquireSharedTexture = nullptr;
    PFNOGLReleaseSharedTextureINTEL glReleaseSharedTexture = nullptr;
    PFNglGetString glGetString = nullptr;
    PFNglGetStringi glGetStringi = nullptr;
    PFNglGetIntegerv glGetIntegerv = nullptr;
    PFNOGLRetainSyncINTEL glRetainSync = nullptr;
    PFNOGLReleaseSyncINTEL glReleaseSync = nullptr;
    PFNOGLGetSyncivINTEL glGetSynciv = nullptr;
    PFNglArbSyncObjectSetup pfnGlArbSyncObjectSetup = nullptr;
    PFNglArbSyncObjectCleanup pfnGlArbSyncObjectCleanup = nullptr;
    PFNglArbSyncObjectSignal pfnGlArbSyncObjectSignal = nullptr;
    PFNglArbSyncObjectWaitServer pfnGlArbSyncObjectWaitServer = nullptr;
    PFNeglCreateImage eglCreateImage = nullptr;
    PFNeglDestroyImage eglDestroyImage = nullptr;
    // support for GL_ARB_cl_event
    std::mutex glArbEventMutex;
    std::unordered_map<Event *, GlArbSyncEvent *> glArbEventMapping;
};

template <typename EventType>
inline auto GLSharingFunctionsLinux::getOrCreateGlArbSyncEvent(Event &baseEvent) -> decltype(EventType::create(baseEvent)) {
    std::lock_guard<std::mutex> lock{glArbEventMutex};
    auto it = glArbEventMapping.find(&baseEvent);
    if (it != glArbEventMapping.end()) {
        return it->second;
    }

    auto arbEvent = EventType::create(baseEvent);
    if (nullptr == arbEvent) {
        return arbEvent;
    }
    glArbEventMapping[&baseEvent] = arbEvent;
    return arbEvent;
}

} // namespace NEO
