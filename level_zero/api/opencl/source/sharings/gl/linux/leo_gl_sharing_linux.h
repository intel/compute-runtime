/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/api/opencl/extensions/public/cl_gl_private_intel.h"
#include "level_zero/api/opencl/source/sharings/gl/leo_gl_sharing.h"
#include "level_zero/api/opencl/source/sharings/gl/linux/include/leo_gl_types.h"

#include "third_party/opengl_headers/GL/mesa_glinterop.h"
#include <GL/gl.h>

#include <EGL/eglext.h>

typedef struct _XDisplay *GLXDisplay;
typedef struct __GLXcontextRec *GLXContext;

namespace NEO {
namespace LEO {

// OpenGL API names
typedef const GLubyte *(*PFNglGetString)(GLenum name);
typedef const GLubyte *(*PFNglGetStringi)(GLenum name, GLuint index);
typedef void (*PFNglGetIntegerv)(GLenum pname, GLint *params);
typedef void (*PFNglBindTexture)(GLenum target, GLuint texture);

typedef bool (*PFNglArbSyncObjectSetup)(GLSharingFunctions &sharing, OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo);
typedef void (*PFNglArbSyncObjectCleanup)(OSInterface &osInterface, CL_GL_SYNC_INFO *glSyncInfo);
typedef void (*PFNglArbSyncObjectSignal)(OsContext &osContext, CL_GL_SYNC_INFO &glSyncInfo);
typedef void (*PFNglArbSyncObjectWaitServer)(OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo, bool cpuSync);

class GLSharingFunctionsLinux : public GLSharingFunctions {
  public:
    GLSharingFunctionsLinux() = default;
    GLSharingFunctionsLinux(GLType glhdcType, GLContext glhglrcHandle, GLContext glhglrcHandleBkpCtx, GLDisplay glhdcHandle);
    ~GLSharingFunctionsLinux() override;

    GLboolean initGLFunctions() override;
    bool isOpenGlSharingSupported() override;

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
    void glArbSyncObjectWaitServer(OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo, bool cpuSync) {
        pfnGlArbSyncObjectWaitServer(osInterface, glSyncInfo, cpuSync);
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
};

} // namespace LEO
} // namespace NEO
