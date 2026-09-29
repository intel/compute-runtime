/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "config.h"
#include <CL/cl.h>

#include <memory>
#include <mutex>
#include <vector>

namespace NEO {

// LEO (Level Zero Executing OpenCL) lets the Level Zero driver service OpenCL for
// products that opt in via ProductHelper::isLEOSupported(). The choice is per
// platform: one libigdrcl serves every Intel GPU on Linux, so a LEO-capable
// product sitting next to one that is not has to split.
//
// clGetPlatformIDs builds its list from both halves:
//   - isLeoRootDevice() classifies each root device. LEO ones get no native
//     bring-up; only their sort key is kept, on the ExecutionEnvironment.
//   - The rest become native NEO::Platform objects, owned by platformsImpl.
//   - libze_intel_gpu is then asked for its platforms. Those handles are kept in
//     leoPlatformEntries and reported to the application verbatim.
//   - Both halves are merged in Device::groupDevices order, which each side
//     produces by grouping one platform per PRODUCT_FAMILY.
//
// Level Zero backed handles are not wrapped. Each carries its own ICD dispatch
// table, so the loader routes its calls - and everything derived from it - into
// libze_intel_gpu directly, keeping clGetDeviceInfo(dev, CL_DEVICE_PLATFORM)
// equal to the handle the application was given.
//
// Two things the loader cannot route still reach this driver. The entry points
// igdrcl exports directly, which an application can call without the loader; and
// everything handed out by clGetExtensionFunctionAddress, which takes no platform
// and so cannot know which backend the caller meant. Both route per call on the
// object they are given: one this driver cannot cast is not ours, and belongs to
// libze_intel_gpu whenever it reported a platform here. The forward* helpers
// below carry those calls across.
//
// EnableLEO overrides the per-platform choice: 0 keeps every product native and
// never loads the Level Zero backend, 1 forces LEO for every platform.

class OsLibrary;

bool isLeoForcedOn();
bool isLeoForcedOff();
bool ensureLeoLibraryLoaded();
std::vector<cl_platform_id> getLeoPlatforms();
bool isLeoPlatformHandle(cl_platform_id platform);
bool areAllPlatformsLeo();
bool hasLeoPlatforms();

cl_int forwardClGetPlatformIDs(cl_uint numEntries, cl_platform_id *platforms, cl_uint *numPlatforms);
cl_int forwardClGetPlatformInfo(cl_platform_id platform, cl_platform_info paramName, size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet);
void *forwardClGetExtensionFunctionAddress(const char *funcName);
cl_int forwardClEnqueueMarkerWithSyncObjectINTEL(cl_command_queue commandQueue, cl_event *event, cl_context *context);
cl_int forwardClGetCLObjectInfoINTEL(cl_mem memObj, void *pResourceInfo);
cl_int forwardClGetCLEventInfoINTEL(cl_event event, void **pSyncInfoHandleRet, cl_context *pClContextRet);
cl_int forwardClReleaseGlSharedEventINTEL(cl_event event);

using pfnClIcdGetPlatformIDsKHR = CL_API_ENTRY cl_int(CL_API_CALL *)(cl_uint, cl_platform_id *, cl_uint *);
using pfnClEnqueueMarkerWithSyncObjectINTEL = cl_int(CL_API_CALL *)(cl_command_queue, cl_event *, cl_context *);
using pfnClGetCLObjectInfoINTEL = cl_int(CL_API_CALL *)(cl_mem, void *);
using pfnClGetCLEventInfoINTEL = cl_int(CL_API_CALL *)(cl_event, void **, cl_context *);
using pfnClReleaseGlSharedEventINTEL = cl_int(CL_API_CALL *)(cl_event);

struct L0ForwardingState {
    std::mutex mutex;
    bool loaded = false;
    std::unique_ptr<OsLibrary> library;
    pfnClIcdGetPlatformIDsKHR clGetPlatformIDsFunc = nullptr;
    decltype(&clGetPlatformInfo) clGetPlatformInfoFunc = nullptr;
    decltype(&clGetExtensionFunctionAddress) clGetExtensionFunctionAddressFunc = nullptr;
    pfnClEnqueueMarkerWithSyncObjectINTEL clEnqueueMarkerWithSyncObjectINTELFunc = nullptr;
    pfnClGetCLObjectInfoINTEL clGetCLObjectInfoINTELFunc = nullptr;
    pfnClGetCLEventInfoINTEL clGetCLEventInfoINTELFunc = nullptr;
    pfnClReleaseGlSharedEventINTEL clReleaseGlSharedEventINTELFunc = nullptr;
};

extern L0ForwardingState *l0ForwardingState;

bool leoForwardingSelfLoad();

void leoSetup();
void leoTeardown();

} // namespace NEO

// Forwards the enclosing entry point to libze_intel_gpu when object is not one of
// ours and a Level Zero backed platform exists. Expanded at the call site, which
// has to see castToObject for castType.
#define FORWARD_TO_LEO_IF_FOREIGN(castType, object, name, ...)                      \
    if (nullptr == NEO::castToObject<castType>(object) && NEO::hasLeoPlatforms()) { \
        if (auto leoFunc = reinterpret_cast<decltype(&name)>(                       \
                NEO::forwardClGetExtensionFunctionAddress(#name))) {                \
            return leoFunc(__VA_ARGS__);                                            \
        }                                                                           \
    }
