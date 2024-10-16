/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sharings/gl/linux/gl_sharing_linux.h"

#include "shared/source/os_interface/linux/sys_calls.h"

#include "opencl/source/context/context.inl"
#include "opencl/source/helpers/gl_helper.h"
#include "opencl/source/sharings/gl/gl_arb_sync_event.h"

namespace NEO {
GLSharingFunctionsLinux::GLSharingFunctionsLinux(GLType glhdcType, GLContext glhglrcHandle, GLContext glhglrcHandleBkpCtx, GLDisplay glhdcHandle)
    : glHDCType(glhdcType), glHGLRCHandle(glhglrcHandle), glHGLRCHandleBkpCtx(glhglrcHandleBkpCtx), glHDCHandle(glhdcHandle) {
    GLSharingFunctionsLinux::initGLFunctions();
}
GLSharingFunctionsLinux::~GLSharingFunctionsLinux() = default;

bool GLSharingFunctionsLinux::isOpenGlExtensionSupported(const unsigned char *pExtensionString) {
    if (glGetStringi == nullptr || glGetIntegerv == nullptr) {
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

bool GLSharingFunctionsLinux::isOpenGlSharingSupported() {

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

GlArbSyncEvent *GLSharingFunctionsLinux::getGlArbSyncEvent(Event &baseEvent) {
    std::lock_guard<std::mutex> lock{glArbEventMutex};
    auto it = glArbEventMapping.find(&baseEvent);
    if (it != glArbEventMapping.end()) {
        return it->second;
    }
    return nullptr;
}

void GLSharingFunctionsLinux::removeGlArbSyncEventMapping(Event &baseEvent) {
    std::lock_guard<std::mutex> lock{glArbEventMutex};
    auto it = glArbEventMapping.find(&baseEvent);
    if (it == glArbEventMapping.end()) {
        DEBUG_BREAK_IF(it == glArbEventMapping.end());
        return;
    }
    glArbEventMapping.erase(it);
}

GLboolean GLSharingFunctionsLinux::initGLFunctions() {
    std::unique_ptr<OsLibrary> dynLibrary(OsLibrary::loadFunc({""}));

    GlFunctionHelper glXGetProc(dynLibrary.get(), "glXGetProcAddress");
    if (glXGetProc.ready()) {
        glXGLInteropQueryDeviceInfo = glXGetProc["glXGLInteropQueryDeviceInfoMESA"];
        glXGLInteropExportObject = glXGetProc["glXGLInteropExportObjectMESA"];
        glXGLInteropFlushObjects = glXGetProc["glXGLInteropFlushObjectsMESA"];
    }

    GlFunctionHelper eglGetProc(dynLibrary.get(), "eglGetProcAddress");
    if (eglGetProc.ready()) {
        eglGLInteropQueryDeviceInfo = eglGetProc["eglGLInteropQueryDeviceInfoMESA"];
        eglGLInteropExportObject = eglGetProc["eglGLInteropExportObjectMESA"];
        eglGLInteropFlushObjects = eglGetProc["eglGLInteropFlushObjectsMESA"];
    }

    glGetString = (*dynLibrary)["glGetString"];
    glGetStringi = (*dynLibrary)["glGetStringi"];
    glGetIntegerv = (*dynLibrary)["glGetIntegerv"];

    this->pfnGlArbSyncObjectCleanup = cleanupArbSyncObject;
    this->pfnGlArbSyncObjectSetup = setupArbSyncObject;
    this->pfnGlArbSyncObjectSignal = signalArbSyncObject;
    this->pfnGlArbSyncObjectWaitServer = serverWaitForArbSyncObject;

    return 1;
}
bool GLSharingFunctionsLinux::flushObjectsAndWait(unsigned count, struct mesa_glinterop_export_in *resources, struct mesa_glinterop_flush_out *out, int *retValPtr) {
    /* Call MESA interop */
    int retValue = flushObjects(1, resources, out);
    if (retValPtr) {
        *retValPtr = retValue;
    }
    if ((retValue != MESA_GLINTEROP_SUCCESS) && (out->version == 1)) {
        return false;
    }
    auto fenceFd = *out->fence_fd;
    /* Wait on the fence fd */
    struct pollfd fp = {
        .fd = fenceFd,
        .events = POLLIN,
        .revents = 0,
    };
    retValue = SysCalls::poll(&fp, 1, 1000);
    SysCalls::close(fenceFd);
    return retValue >= 0;
}

template GLSharingFunctionsLinux *Context::getSharing<GLSharingFunctionsLinux>();

} // namespace NEO
