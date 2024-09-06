/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/extensions/public/cl_gl_private_intel.h"
#include "opencl/source/sharings/gl/gl_sharing.h"
#include "opencl/source/sharings/gl/linux/include/gl_types.h"

#include "third_party/opengl_headers/GL/mesa_glinterop.h"
#include <GL/gl.h>

#include <EGL/eglext.h>

typedef struct _XDisplay *GLXDisplay;
typedef struct __GLXcontextRec *GLXContext;

namespace NEO {
// OpenGL API names
typedef const GLubyte *(*PFNglGetString)(GLenum name);
typedef const GLubyte *(*PFNglGetStringi)(GLenum name, GLuint index);
typedef void (*PFNglGetIntegerv)(GLenum pname, GLint *params);
typedef void (*PFNglBindTexture)(GLenum target, GLuint texture);

typedef bool (*PFNglArbSyncObjectSetup)(GLSharingFunctions &sharing, OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo);
typedef void (*PFNglArbSyncObjectCleanup)(OSInterface &osInterface, CL_GL_SYNC_INFO *glSyncInfo);
typedef void (*PFNglArbSyncObjectSignal)(OsContext &osContext, CL_GL_SYNC_INFO &glSyncInfo);
typedef void (*PFNglArbSyncObjectWaitServer)(OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo);

class GLSharingFunctionsLinux : public GLSharingFunctions {
  public:
    GLSharingFunctionsLinux() = default;
    GLSharingFunctionsLinux(GLType glhdcType, GLContext glhglrcHandle, GLContext glhglrcHandleBkpCtx, GLDisplay glhdcHandle);
    ~GLSharingFunctionsLinux() override;

    GLboolean initGLFunctions() override;
    bool isOpenGlSharingSupported() override;

    // Arb sync event
    template <typename EventType = GlArbSyncEvent>
    auto getOrCreateGlArbSyncEvent(Event &baseEvent) -> decltype(EventType::create(baseEvent));
    GlArbSyncEvent *getGlArbSyncEvent(Event &baseEvent);
    void removeGlArbSyncEventMapping(Event &baseEvent);

    // Gl functions
    int queryDeviceInfo(struct mesa_glinterop_device_info *out) {
        switch (glHDCType) {
        case CL_GLX_DISPLAY_KHR:
            return glXGLInteropQueryDeviceInfo(
                reinterpret_cast<GLXDisplay>(glHDCHandle),
                reinterpret_cast<GLXContext>(glHGLRCHandle),
                out);
        case CL_EGL_DISPLAY_KHR:
            return eglGLInteropQueryDeviceInfo(
                reinterpret_cast<EGLDisplay>(glHDCHandle),
                reinterpret_cast<EGLContext>(glHGLRCHandle),
                out);
        default:
            return -ENOTSUP;
        }
    }
    int exportObject(struct mesa_glinterop_export_in *in, struct mesa_glinterop_export_out *out) {
        switch (glHDCType) {
        case CL_GLX_DISPLAY_KHR:
            return glXGLInteropExportObject(
                reinterpret_cast<GLXDisplay>(glHDCHandle),
                reinterpret_cast<GLXContext>(glHGLRCHandle),
                in, out);
        case CL_EGL_DISPLAY_KHR:
            return eglGLInteropExportObject(
                reinterpret_cast<EGLDisplay>(glHDCHandle),
                reinterpret_cast<EGLContext>(glHGLRCHandle),
                in, out);
        default:
            return -ENOTSUP;
        }
    }
    int flushObjects(unsigned count, struct mesa_glinterop_export_in *resources, struct mesa_glinterop_flush_out *out) {
        switch (glHDCType) {
        case CL_GLX_DISPLAY_KHR:
            return glXGLInteropFlushObjects(
                reinterpret_cast<GLXDisplay>(glHDCHandle),
                reinterpret_cast<GLXContext>(glHGLRCHandle),
                count, resources, out);
        case CL_EGL_DISPLAY_KHR:
            return eglGLInteropFlushObjects(
                reinterpret_cast<EGLDisplay>(glHDCHandle),
                reinterpret_cast<EGLContext>(glHGLRCHandle),
                count, resources, out);
        default:
            return -ENOTSUP;
        }
    }
    bool flushObjectsAndWait(unsigned count, struct mesa_glinterop_export_in *resources, struct mesa_glinterop_flush_out *out, int *retValPtr = nullptr);
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

  protected:
    bool isOpenGlExtensionSupported(const unsigned char *pExtentionString);

    // Handles
    GLType glHDCType = 0;
    GLContext glHGLRCHandle = 0;
    GLContext glHGLRCHandleBkpCtx = 0;
    GLDisplay glHDCHandle = 0;

    // GL functions
    PFNglGetString glGetString = nullptr;
    PFNglGetStringi glGetStringi = nullptr;
    PFNglGetIntegerv glGetIntegerv = nullptr;
    PFNMESAGLINTEROPGLXQUERYDEVICEINFOPROC glXGLInteropQueryDeviceInfo = nullptr;
    PFNMESAGLINTEROPGLXEXPORTOBJECTPROC glXGLInteropExportObject = nullptr;
    PFNMESAGLINTEROPGLXFLUSHOBJECTSPROC glXGLInteropFlushObjects = nullptr;
    PFNMESAGLINTEROPEGLQUERYDEVICEINFOPROC eglGLInteropQueryDeviceInfo = nullptr;
    PFNMESAGLINTEROPEGLEXPORTOBJECTPROC eglGLInteropExportObject = nullptr;
    PFNMESAGLINTEROPEGLFLUSHOBJECTSPROC eglGLInteropFlushObjects = nullptr;
    PFNglArbSyncObjectSetup pfnGlArbSyncObjectSetup = nullptr;
    PFNglArbSyncObjectCleanup pfnGlArbSyncObjectCleanup = nullptr;
    PFNglArbSyncObjectSignal pfnGlArbSyncObjectSignal = nullptr;
    PFNglArbSyncObjectWaitServer pfnGlArbSyncObjectWaitServer = nullptr;
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
