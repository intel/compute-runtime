/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "config.h"
#include <CL/cl.h>

namespace NEO {

bool isLEOEnabled();

cl_int forwardClGetPlatformIDs(cl_uint numEntries, cl_platform_id *platforms, cl_uint *numPlatforms);
cl_int forwardClGetPlatformInfo(cl_platform_id platform, cl_platform_info paramName, size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet);
cl_int forwardClGetDeviceIDs(cl_platform_id platform, cl_device_type deviceType, cl_uint numEntries, cl_device_id *devices, cl_uint *numDevices);
void *forwardClGetExtensionFunctionAddress(const char *funcName);
cl_int forwardClEnqueueMarkerWithSyncObjectINTEL(cl_command_queue commandQueue, cl_event *event, cl_context *context);
cl_int forwardClGetCLObjectInfoINTEL(cl_mem memObj, void *pResourceInfo);
cl_int forwardClGetCLEventInfoINTEL(cl_event event, void **pSyncInfoHandleRet, cl_context *pClContextRet);
cl_int forwardClReleaseGlSharedEventINTEL(cl_event event);

void releaseL0Library();

} // namespace NEO
