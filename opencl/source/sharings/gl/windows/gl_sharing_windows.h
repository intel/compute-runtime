/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/source/sharings/gl/gl_sharing.h"

#include "gl_types.h"
#include <GL/gl.h>

namespace NEO {
//OpenGL API names
typedef GLboolean(OSAPI *PFNOGLSetSharedOCLContextStateINTEL)(GLDisplay hdcHandle, GLContext contextHandle, GLboolean state, GLvoid *pContextInfo);
typedef GLboolean(OSAPI *PFNOGLAcquireSharedBufferINTEL)(GLDisplay hdcHandle, GLContext contextHandle, GLContext backupContextHandle, GLvoid *pBufferInfo);
typedef GLboolean(OSAPI *PFNOGLAcquireSharedRenderBufferINTEL)(GLDisplay hdcHandle, GLContext contextHandle, GLContext backupContextHandle, GLvoid *pResourceInfo);
typedef GLboolean(OSAPI *PFNOGLAcquireSharedTextureINTEL)(GLDisplay hdcHandle, GLContext contextHandle, GLContext backupContextHandle, GLvoid *pResourceInfo);
typedef GLboolean(OSAPI *PFNOGLReleaseSharedBufferINTEL)(GLDisplay hdcHandle, GLContext contextHandle, GLContext backupContextHandle, GLvoid *pBufferInfo);
typedef GLboolean(OSAPI *PFNOGLReleaseSharedRenderBufferINTEL)(GLDisplay hdcHandle, GLContext contextHandle, GLContext backupContextHandle, GLvoid *pResourceInfo);
typedef GLboolean(OSAPI *PFNOGLReleaseSharedTextureINTEL)(GLDisplay hdcHandle, GLContext contextHandle, GLContext backupContextHandle, GLvoid *pResourceInfo);
typedef GLContext(OSAPI *PFNOGLGetCurrentContext)();
typedef GLDisplay(OSAPI *PFNOGLGetCurrentDisplay)();
typedef GLboolean(OSAPI *PFNOGLMakeCurrent)(GLDisplay hdcHandle, void *draw, void *read, GLContext contextHandle);
typedef GLboolean(OSAPI *PFNOGLRetainSyncINTEL)(GLDisplay hdcHandle, GLContext contextHandle, GLContext backupContextHandle, GLvoid *pSyncInfo);
typedef GLboolean(OSAPI *PFNOGLReleaseSyncINTEL)(GLDisplay hdcHandle, GLContext contextHandle, GLContext backupContextHandle, GLvoid *pSync);
typedef void(OSAPI *PFNOGLGetSyncivINTEL)(GLvoid *pSync, GLenum pname, GLint *value);

typedef const GLubyte *(OSAPI *PFNglGetString)(GLenum name);
typedef const GLubyte *(OSAPI *PFNglGetStringi)(GLenum name, GLuint index);
typedef void(OSAPI *PFNglGetIntegerv)(GLenum pname, GLint *params);
typedef void(OSAPI *PFNglBindTexture)(GLenum target, GLuint texture);

//wgl
typedef BOOL(OSAPI *PFNwglMakeCurrent)(HDC, HGLRC);
typedef GLContext(OSAPI *PFNwglCreateContext)(GLDisplay hdcHandle);
typedef int(OSAPI *PFNwglShareLists)(GLContext contextHandle, GLContext backupContextHandle);
typedef BOOL(OSAPI *PFNwglDeleteContext)(HGLRC hglrcHandle);

typedef bool (*PFNglArbSyncObjectSetup)(GLSharingFunctions &sharing, OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo);
typedef void (*PFNglArbSyncObjectCleanup)(OSInterface &osInterface, CL_GL_SYNC_INFO *glSyncInfo);
typedef void (*PFNglArbSyncObjectSignal)(OsContext &osContext, CL_GL_SYNC_INFO &glSyncInfo);
typedef void (*PFNglArbSyncObjectWaitServer)(OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo);

class GLSharingFunctionsWindows : public GLSharingFunctions {
  public:
    GLSharingFunctionsWindows() = default;
    GLSharingFunctionsWindows(GLType glhdcType, GLContext glhglrcHandle, GLContext glhglrcHandleBkpCtx, GLDisplay glhdcHandle);
    ~GLSharingFunctionsWindows() override;

    OS_HANDLE getGLDeviceHandle() const { return GLDeviceHandle; }
    OS_HANDLE getGLContextHandle() const { return GLContextHandle; }

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
        return GLAcquireSharedBuffer(GLHDCHandle, GLHGLRCHandle, GLHGLRCHandleBkpCtx, pBufferInfo);
    }
    GLboolean releaseSharedBufferINTEL(GLvoid *pBufferInfo) {
        return GLReleaseSharedBuffer(GLHDCHandle, GLHGLRCHandle, GLHGLRCHandleBkpCtx, pBufferInfo);
    }
    GLboolean acquireSharedRenderBuffer(GLvoid *pResourceInfo) {
        return GLAcquireSharedRenderBuffer(GLHDCHandle, GLHGLRCHandle, GLHGLRCHandleBkpCtx, pResourceInfo);
    }
    GLboolean releaseSharedRenderBuffer(GLvoid *pResourceInfo) {
        return GLReleaseSharedRenderBuffer(GLHDCHandle, GLHGLRCHandle, GLHGLRCHandleBkpCtx, pResourceInfo);
    }
    GLboolean acquireSharedTexture(GLvoid *pResourceInfo) {
        return GLAcquireSharedTexture(GLHDCHandle, GLHGLRCHandle, GLHGLRCHandleBkpCtx, pResourceInfo);
    }
    GLboolean releaseSharedTexture(GLvoid *pResourceInfo) {
        return GLReleaseSharedTexture(GLHDCHandle, GLHGLRCHandle, GLHGLRCHandleBkpCtx, pResourceInfo);
    }
    GLboolean retainSync(GLvoid *pSyncInfo) {
        return GLRetainSync(GLHDCHandle, GLHGLRCHandle, GLHGLRCHandleBkpCtx, pSyncInfo);
    }
    GLboolean releaseSync(GLvoid *pSync) {
        return GLReleaseSync(GLHDCHandle, GLHGLRCHandle, GLHGLRCHandleBkpCtx, pSync);
    }
    void getSynciv(GLvoid *pSync, GLenum pname, GLint *value) {
        return GLGetSynciv(pSync, pname, value);
    }
    GLContext getCurrentContext() {
        return GLGetCurrentContext();
    }
    GLDisplay getCurrentDisplay() {
        return GLGetCurrentDisplay();
    }
    GLboolean makeCurrent(GLContext contextHandle, GLDisplay displayHandle = 0) {
        if (displayHandle == 0) {
            displayHandle = GLHDCHandle;
        }
        return this->wglMakeCurrent(displayHandle, contextHandle);
    }
    GLContext getBackupContextHandle() {
        return GLHGLRCHandleBkpCtx;
    }
    GLContext getContextHandle() {
        return GLHGLRCHandle;
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

  protected:
    void updateOpenGLContext() {
        if (GLSetSharedOCLContextState) {
            setSharedOCLContextState();
        }
    }
    GLboolean setSharedOCLContextState();
    void createBackupContext();
    bool isOpenGlExtensionSupported(const unsigned char *pExtentionString);

    // Handles
    GLType GLHDCType = 0;
    GLContext GLHGLRCHandle = 0;
    GLContext GLHGLRCHandleBkpCtx = 0;
    GLDisplay GLHDCHandle = 0;
    OS_HANDLE GLDeviceHandle = 0;
    OS_HANDLE GLContextHandle = 0;

    // GL functions
    std::unique_ptr<OsLibrary> glLibrary;
    PFNOGLSetSharedOCLContextStateINTEL GLSetSharedOCLContextState = nullptr;
    PFNOGLAcquireSharedBufferINTEL GLAcquireSharedBuffer = nullptr;
    PFNOGLReleaseSharedBufferINTEL GLReleaseSharedBuffer = nullptr;
    PFNOGLAcquireSharedRenderBufferINTEL GLAcquireSharedRenderBuffer = nullptr;
    PFNOGLReleaseSharedRenderBufferINTEL GLReleaseSharedRenderBuffer = nullptr;
    PFNOGLAcquireSharedTextureINTEL GLAcquireSharedTexture = nullptr;
    PFNOGLReleaseSharedTextureINTEL GLReleaseSharedTexture = nullptr;
    PFNOGLGetCurrentContext GLGetCurrentContext = nullptr;
    PFNOGLGetCurrentDisplay GLGetCurrentDisplay = nullptr;
    PFNglGetString glGetString = nullptr;
    PFNglGetStringi glGetStringi = nullptr;
    PFNglGetIntegerv glGetIntegerv = nullptr;
    PFNwglCreateContext pfnWglCreateContext = nullptr;
    PFNwglMakeCurrent wglMakeCurrent = nullptr;
    PFNwglShareLists pfnWglShareLists = nullptr;
    PFNwglDeleteContext pfnWglDeleteContext = nullptr;
    PFNOGLRetainSyncINTEL GLRetainSync = nullptr;
    PFNOGLReleaseSyncINTEL GLReleaseSync = nullptr;
    PFNOGLGetSyncivINTEL GLGetSynciv = nullptr;
    PFNglArbSyncObjectSetup pfnGlArbSyncObjectSetup = nullptr;
    PFNglArbSyncObjectCleanup pfnGlArbSyncObjectCleanup = nullptr;
    PFNglArbSyncObjectSignal pfnGlArbSyncObjectSignal = nullptr;
    PFNglArbSyncObjectWaitServer pfnGlArbSyncObjectWaitServer = nullptr;

    // support for GL_ARB_cl_event
    std::mutex glArbEventMutex;
    std::unordered_map<Event *, GlArbSyncEvent *> glArbEventMapping;
};

template <typename EventType>
inline auto GLSharingFunctionsWindows::getOrCreateGlArbSyncEvent(Event &baseEvent) -> decltype(EventType::create(baseEvent)) {
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
