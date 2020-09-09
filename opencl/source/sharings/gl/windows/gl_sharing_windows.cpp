/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sharings/gl/windows/gl_sharing_windows.h"

#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

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

    initAdapterLuid();

    return 1;
}

LUID GLSharingFunctionsWindows::getAdapterLuid() const {
    return adapterLuid;
}
void GLSharingFunctionsWindows::initAdapterLuid() {
    if (adapterLuid.HighPart != 0 || adapterLuid.LowPart != 0) {
        return;
    }
    WCHAR displayName[ARRAYSIZE(DISPLAY_DEVICEW::DeviceName)];
    UINT iDevNum = 0u;
    DISPLAY_DEVICEW dispDevice = {0};
    dispDevice.cb = sizeof(dispDevice);
    while (SysCalls::enumDisplayDevices(NULL, iDevNum++, &dispDevice, EDD_GET_DEVICE_INTERFACE_NAME)) {
        if (dispDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
            wcscpy_s(displayName, ARRAYSIZE(DISPLAY_DEVICEW::DeviceName), dispDevice.DeviceName);
            break;
        }
    }

    DXGI_ADAPTER_DESC1 OpenAdapterDesc = {{0}};
    DXGI_OUTPUT_DESC outputDesc = {0};
    IDXGIFactory1 *pFactory = nullptr;
    IDXGIAdapter1 *pAdapter = nullptr;
    bool found = false;

    HRESULT hr = Wddm::createDxgiFactory(__uuidof(IDXGIFactory), (void **)(&pFactory));
    if ((hr != S_OK) || (pFactory == nullptr)) {
        return;
    }
    iDevNum = 0u;
    while (pFactory->EnumAdapters1(iDevNum++, &pAdapter) != DXGI_ERROR_NOT_FOUND) {
        IDXGIOutput *pOutput = nullptr;
        UINT outputNum = 0;
        while (pAdapter->EnumOutputs(outputNum++, &pOutput) != DXGI_ERROR_NOT_FOUND && pOutput) {
            pOutput->GetDesc(&outputDesc);
            if (wcscmp(outputDesc.DeviceName, displayName) == 0) {

                hr = pAdapter->GetDesc1(&OpenAdapterDesc);
                if (hr == S_OK) {
                    adapterLuid = OpenAdapterDesc.AdapterLuid;
                    found = true;
                    break;
                }
            }
        }
        pAdapter->Release();
        pAdapter = nullptr;
        if (found) {
            break;
        }
    }

    if (pFactory != nullptr) {
        pFactory->Release();
        pFactory = nullptr;
    }
}

template GLSharingFunctionsWindows *Context::getSharing<GLSharingFunctionsWindows>();

} // namespace NEO
