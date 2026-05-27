/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/api/leo_forwarding.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/os_library.h"

#include <mutex>

namespace NEO {

bool isLEOEnabled() {
    return debugManager.flags.EnableLEO.get() == 1;
}

std::mutex l0LibraryMutex;
bool l0LibraryLoaded = false;
std::unique_ptr<OsLibrary> l0Library;
decltype(&clGetPlatformIDs) l0ClGetPlatformIDs = nullptr;
decltype(&clGetPlatformInfo) l0ClGetPlatformInfo = nullptr;
decltype(&clGetDeviceIDs) l0ClGetDeviceIDs = nullptr;
decltype(&clGetExtensionFunctionAddress) l0ClGetExtensionFunctionAddress = nullptr;

using pfnClEnqueueMarkerWithSyncObjectINTEL = cl_int(CL_API_CALL *)(cl_command_queue, cl_event *, cl_context *);
using pfnClGetCLObjectInfoINTEL = cl_int(CL_API_CALL *)(cl_mem, void *);
using pfnClGetCLEventInfoINTEL = cl_int(CL_API_CALL *)(cl_event, void **, cl_context *);
using pfnClReleaseGlSharedEventINTEL = cl_int(CL_API_CALL *)(cl_event);

pfnClEnqueueMarkerWithSyncObjectINTEL l0ClEnqueueMarkerWithSyncObjectINTEL = nullptr;
pfnClGetCLObjectInfoINTEL l0ClGetCLObjectInfoINTEL = nullptr;
pfnClGetCLEventInfoINTEL l0ClGetCLEventInfoINTEL = nullptr;
pfnClReleaseGlSharedEventINTEL l0ClReleaseGlSharedEventINTEL = nullptr;

static void loadL0Library() {
    if (l0LibraryLoaded) {
        return;
    }

    std::lock_guard<std::mutex> lock(l0LibraryMutex);
    if (l0LibraryLoaded) {
        return;
    }

    l0LibraryLoaded = true;
    static constexpr const char *l0LibraryName =
#if defined(_WIN64)
        "ze_intel_gpu64.dll";
#else
        "libze_intel_gpu.so.1";
#endif
    OsLibraryCreateProperties properties(l0LibraryName);
    l0Library.reset(OsLibrary::loadFunc(properties));
    if (l0Library && l0Library->isLoaded()) {
        l0ClGetPlatformIDs = reinterpret_cast<decltype(&clGetPlatformIDs)>(l0Library->getProcAddress("clGetPlatformIDs"));
        l0ClGetPlatformInfo = reinterpret_cast<decltype(&clGetPlatformInfo)>(l0Library->getProcAddress("clGetPlatformInfo"));
        l0ClGetDeviceIDs = reinterpret_cast<decltype(&clGetDeviceIDs)>(l0Library->getProcAddress("clGetDeviceIDs"));
        l0ClGetExtensionFunctionAddress = reinterpret_cast<decltype(&clGetExtensionFunctionAddress)>(l0Library->getProcAddress("clGetExtensionFunctionAddress"));
        l0ClEnqueueMarkerWithSyncObjectINTEL = reinterpret_cast<pfnClEnqueueMarkerWithSyncObjectINTEL>(l0Library->getProcAddress("clEnqueueMarkerWithSyncObjectINTEL"));
        l0ClGetCLObjectInfoINTEL = reinterpret_cast<pfnClGetCLObjectInfoINTEL>(l0Library->getProcAddress("clGetCLObjectInfoINTEL"));
        l0ClGetCLEventInfoINTEL = reinterpret_cast<pfnClGetCLEventInfoINTEL>(l0Library->getProcAddress("clGetCLEventInfoINTEL"));
        l0ClReleaseGlSharedEventINTEL = reinterpret_cast<pfnClReleaseGlSharedEventINTEL>(l0Library->getProcAddress("clReleaseGlSharedEventINTEL"));
    }
}

cl_int forwardClGetPlatformIDs(cl_uint numEntries, cl_platform_id *platforms, cl_uint *numPlatforms) {
    loadL0Library();
    if (l0ClGetPlatformIDs) [[likely]] {
        return l0ClGetPlatformIDs(numEntries, platforms, numPlatforms);
    }
    if (numPlatforms) {
        *numPlatforms = 0u;
    }
    return CL_SUCCESS;
}

cl_int forwardClGetPlatformInfo(cl_platform_id platform, cl_platform_info paramName, size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet) {
    loadL0Library();
    if (l0ClGetPlatformInfo) [[likely]] {
        return l0ClGetPlatformInfo(platform, paramName, paramValueSize, paramValue, paramValueSizeRet);
    }
    return CL_INVALID_PLATFORM;
}

cl_int forwardClGetDeviceIDs(cl_platform_id platform, cl_device_type deviceType, cl_uint numEntries, cl_device_id *devices, cl_uint *numDevices) {
    loadL0Library();
    if (l0ClGetDeviceIDs) [[likely]] {
        return l0ClGetDeviceIDs(platform, deviceType, numEntries, devices, numDevices);
    }
    return CL_DEVICE_NOT_FOUND;
}

void *forwardClGetExtensionFunctionAddress(const char *funcName) {
    loadL0Library();
    if (l0ClGetExtensionFunctionAddress) [[likely]] {
        return l0ClGetExtensionFunctionAddress(funcName);
    }
    return nullptr;
}

cl_int forwardClEnqueueMarkerWithSyncObjectINTEL(cl_command_queue commandQueue, cl_event *event, cl_context *context) {
    loadL0Library();
    if (l0ClEnqueueMarkerWithSyncObjectINTEL) [[likely]] {
        return l0ClEnqueueMarkerWithSyncObjectINTEL(commandQueue, event, context);
    }
    return CL_INVALID_OPERATION;
}

cl_int forwardClGetCLObjectInfoINTEL(cl_mem memObj, void *pResourceInfo) {
    loadL0Library();
    if (l0ClGetCLObjectInfoINTEL) [[likely]] {
        return l0ClGetCLObjectInfoINTEL(memObj, pResourceInfo);
    }
    return CL_INVALID_OPERATION;
}

cl_int forwardClGetCLEventInfoINTEL(cl_event event, void **pSyncInfoHandleRet, cl_context *pClContextRet) {
    loadL0Library();
    if (l0ClGetCLEventInfoINTEL) [[likely]] {
        return l0ClGetCLEventInfoINTEL(event, pSyncInfoHandleRet, pClContextRet);
    }
    return CL_INVALID_OPERATION;
}

cl_int forwardClReleaseGlSharedEventINTEL(cl_event event) {
    loadL0Library();
    if (l0ClReleaseGlSharedEventINTEL) [[likely]] {
        return l0ClReleaseGlSharedEventINTEL(event);
    }
    return CL_INVALID_OPERATION;
}

void releaseL0Library() {
    std::lock_guard<std::mutex> lock(l0LibraryMutex);
    l0Library.reset();
    l0LibraryLoaded = false;
    l0ClGetPlatformIDs = nullptr;
    l0ClGetPlatformInfo = nullptr;
    l0ClGetDeviceIDs = nullptr;
    l0ClGetExtensionFunctionAddress = nullptr;
    l0ClEnqueueMarkerWithSyncObjectINTEL = nullptr;
    l0ClGetCLObjectInfoINTEL = nullptr;
    l0ClGetCLEventInfoINTEL = nullptr;
    l0ClReleaseGlSharedEventINTEL = nullptr;
}

} // namespace NEO
