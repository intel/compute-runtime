/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/api/leo_forwarding.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/os_library.h"

namespace NEO {

static void loadL0Library();

bool isLEOEnabled() {
    auto flag = debugManager.flags.EnableLEO.get();
    if (flag == 0) {
        return false;
    }
    loadL0Library();
    if (flag == 1) {
        return true;
    }
    return l0ForwardingState && l0ForwardingState->hasPlatforms;
}

L0ForwardingState *l0ForwardingState = nullptr;

void leoSetup() {
    l0ForwardingState = new L0ForwardingState;
}

void leoTeardown() {
    delete l0ForwardingState;
    l0ForwardingState = nullptr;
}

static void loadL0Library() {
    if (!l0ForwardingState || l0ForwardingState->loaded) {
        return;
    }

    std::lock_guard<std::mutex> lock(l0ForwardingState->mutex);
    if (l0ForwardingState->loaded) {
        return;
    }

    l0ForwardingState->loaded = true;
    static constexpr const char *l0LibraryName =
#if defined(_WIN64)
        "ze_intel_gpu64.dll";
#else
        "libze_intel_gpu.so.1";
#endif
    OsLibraryCreateProperties properties(l0LibraryName);
    l0ForwardingState->library.reset(OsLibrary::loadFunc(properties));
    if (l0ForwardingState->library && l0ForwardingState->library->isLoaded()) {
        l0ForwardingState->clGetPlatformIDsFunc = reinterpret_cast<decltype(&clGetPlatformIDs)>(l0ForwardingState->library->getProcAddress("clGetPlatformIDs"));
        l0ForwardingState->clGetPlatformInfoFunc = reinterpret_cast<decltype(&clGetPlatformInfo)>(l0ForwardingState->library->getProcAddress("clGetPlatformInfo"));
        l0ForwardingState->clGetDeviceIDsFunc = reinterpret_cast<decltype(&clGetDeviceIDs)>(l0ForwardingState->library->getProcAddress("clGetDeviceIDs"));
        l0ForwardingState->clGetExtensionFunctionAddressFunc = reinterpret_cast<decltype(&clGetExtensionFunctionAddress)>(l0ForwardingState->library->getProcAddress("clGetExtensionFunctionAddress"));
        l0ForwardingState->clEnqueueMarkerWithSyncObjectINTELFunc = reinterpret_cast<pfnClEnqueueMarkerWithSyncObjectINTEL>(l0ForwardingState->library->getProcAddress("clEnqueueMarkerWithSyncObjectINTEL"));
        l0ForwardingState->clGetCLObjectInfoINTELFunc = reinterpret_cast<pfnClGetCLObjectInfoINTEL>(l0ForwardingState->library->getProcAddress("clGetCLObjectInfoINTEL"));
        l0ForwardingState->clGetCLEventInfoINTELFunc = reinterpret_cast<pfnClGetCLEventInfoINTEL>(l0ForwardingState->library->getProcAddress("clGetCLEventInfoINTEL"));
        l0ForwardingState->clReleaseGlSharedEventINTELFunc = reinterpret_cast<pfnClReleaseGlSharedEventINTEL>(l0ForwardingState->library->getProcAddress("clReleaseGlSharedEventINTEL"));

        if (l0ForwardingState->clGetPlatformIDsFunc) {
            cl_uint numPlatforms = 0u;
            l0ForwardingState->clGetPlatformIDsFunc(0, nullptr, &numPlatforms);
            l0ForwardingState->hasPlatforms = (numPlatforms > 0u);
        }
    }
}

cl_int forwardClGetPlatformIDs(cl_uint numEntries, cl_platform_id *platforms, cl_uint *numPlatforms) {
    if (l0ForwardingState && l0ForwardingState->clGetPlatformIDsFunc) [[likely]] {
        return l0ForwardingState->clGetPlatformIDsFunc(numEntries, platforms, numPlatforms);
    }
    if (numPlatforms) {
        *numPlatforms = 0u;
    }
    return CL_SUCCESS;
}

cl_int forwardClGetPlatformInfo(cl_platform_id platform, cl_platform_info paramName, size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet) {
    if (l0ForwardingState && l0ForwardingState->clGetPlatformInfoFunc) [[likely]] {
        return l0ForwardingState->clGetPlatformInfoFunc(platform, paramName, paramValueSize, paramValue, paramValueSizeRet);
    }
    return CL_INVALID_PLATFORM;
}

cl_int forwardClGetDeviceIDs(cl_platform_id platform, cl_device_type deviceType, cl_uint numEntries, cl_device_id *devices, cl_uint *numDevices) {
    if (l0ForwardingState && l0ForwardingState->clGetDeviceIDsFunc) [[likely]] {
        return l0ForwardingState->clGetDeviceIDsFunc(platform, deviceType, numEntries, devices, numDevices);
    }
    return CL_DEVICE_NOT_FOUND;
}

void *forwardClGetExtensionFunctionAddress(const char *funcName) {
    if (l0ForwardingState && l0ForwardingState->clGetExtensionFunctionAddressFunc) [[likely]] {
        return l0ForwardingState->clGetExtensionFunctionAddressFunc(funcName);
    }
    return nullptr;
}

cl_int forwardClEnqueueMarkerWithSyncObjectINTEL(cl_command_queue commandQueue, cl_event *event, cl_context *context) {
    if (l0ForwardingState && l0ForwardingState->clEnqueueMarkerWithSyncObjectINTELFunc) [[likely]] {
        return l0ForwardingState->clEnqueueMarkerWithSyncObjectINTELFunc(commandQueue, event, context);
    }
    return CL_INVALID_OPERATION;
}

cl_int forwardClGetCLObjectInfoINTEL(cl_mem memObj, void *pResourceInfo) {
    if (l0ForwardingState && l0ForwardingState->clGetCLObjectInfoINTELFunc) [[likely]] {
        return l0ForwardingState->clGetCLObjectInfoINTELFunc(memObj, pResourceInfo);
    }
    return CL_INVALID_OPERATION;
}

cl_int forwardClGetCLEventInfoINTEL(cl_event event, void **pSyncInfoHandleRet, cl_context *pClContextRet) {
    if (l0ForwardingState && l0ForwardingState->clGetCLEventInfoINTELFunc) [[likely]] {
        return l0ForwardingState->clGetCLEventInfoINTELFunc(event, pSyncInfoHandleRet, pClContextRet);
    }
    return CL_INVALID_OPERATION;
}

cl_int forwardClReleaseGlSharedEventINTEL(cl_event event) {
    if (l0ForwardingState && l0ForwardingState->clReleaseGlSharedEventINTELFunc) [[likely]] {
        return l0ForwardingState->clReleaseGlSharedEventINTELFunc(event);
    }
    return CL_INVALID_OPERATION;
}

} // namespace NEO
