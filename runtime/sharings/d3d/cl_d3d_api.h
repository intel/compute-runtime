/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "CL/cl.h"
#include <d3d10_1.h>
#include "CL/cl_dx9_media_sharing.h"
#include "CL/cl_d3d10.h"
#include "CL/cl_d3d11.h"
#define CL_DX9_MEDIA_SHARING_INTEL_EXT
#include "CL/cl_dx9_media_sharing_intel.h"

extern CL_API_ENTRY cl_int CL_API_CALL clGetDeviceIDsFromDX9INTEL(
    cl_platform_id platform,
    cl_dx9_device_source_intel dx9DeviceSource,
    void *dx9Object,
    cl_dx9_device_set_intel dx9DeviceSet,
    cl_uint numEntries,
    cl_device_id *devices,
    cl_uint *numDevices);

extern CL_API_ENTRY cl_mem CL_API_CALL clCreateFromDX9MediaSurfaceINTEL(
    cl_context context,
    cl_mem_flags flags,
    IDirect3DSurface9 *resource,
    HANDLE sharedHandle,
    UINT plane,
    cl_int *errcodeRet);

extern CL_API_ENTRY cl_int CL_API_CALL clEnqueueAcquireDX9ObjectsINTEL(
    cl_command_queue commandQueue,
    cl_uint numObjects,
    const cl_mem *memObjects,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event);

extern CL_API_ENTRY cl_int CL_API_CALL clEnqueueReleaseDX9ObjectsINTEL(
    cl_command_queue commandQueue,
    cl_uint numObjects,
    cl_mem *memObjects,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event);

extern CL_API_ENTRY cl_int CL_API_CALL clGetDeviceIDsFromDX9MediaAdapterKHR(
    cl_platform_id platform,
    cl_uint numMediaAdapters,
    cl_dx9_media_adapter_type_khr *mediaAdapterType,
    void *mediaAdapters,
    cl_dx9_media_adapter_set_khr mediaAdapterSet,
    cl_uint numEntries,
    cl_device_id *devices,
    cl_uint *numDevices) CL_API_SUFFIX__VERSION_1_2;

extern CL_API_ENTRY cl_mem CL_API_CALL clCreateFromDX9MediaSurfaceKHR(
    cl_context context,
    cl_mem_flags flags,
    cl_dx9_media_adapter_type_khr adapterType,
    void *surfaceInfo,
    cl_uint plane,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_2;

extern CL_API_ENTRY cl_int CL_API_CALL clEnqueueAcquireDX9MediaSurfacesKHR(
    cl_command_queue commandQueue,
    cl_uint numObjects,
    const cl_mem *memObjects,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_2;

extern CL_API_ENTRY cl_int CL_API_CALL clEnqueueReleaseDX9MediaSurfacesKHR(
    cl_command_queue commandQueue,
    cl_uint numObjects,
    const cl_mem *memObjects,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_2;

extern CL_API_ENTRY cl_int CL_API_CALL
clGetDeviceIDsFromD3D10KHR(
    cl_platform_id platform,
    cl_d3d10_device_source_khr d3dDeviceSource,
    void *d3dObject,
    cl_d3d10_device_set_khr d3dDeviceSet,
    cl_uint numEntries,
    cl_device_id *devices,
    cl_uint *numDevices) CL_API_SUFFIX__VERSION_1_0;

extern CL_API_ENTRY cl_mem CL_API_CALL
clCreateFromD3D10BufferKHR(
    cl_context context,
    cl_mem_flags flags,
    ID3D10Buffer *resource,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_0;

extern CL_API_ENTRY cl_mem CL_API_CALL
clCreateFromD3D10Texture2DKHR(
    cl_context context,
    cl_mem_flags flags,
    ID3D10Texture2D *resource,
    UINT subresource,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_0;

extern CL_API_ENTRY cl_mem CL_API_CALL
clCreateFromD3D10Texture3DKHR(
    cl_context context,
    cl_mem_flags flags,
    ID3D10Texture3D *resource,
    UINT subresource,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_0;

extern CL_API_ENTRY cl_int CL_API_CALL
clEnqueueAcquireD3D10ObjectsKHR(
    cl_command_queue commandQueue,
    cl_uint numObjects,
    const cl_mem *memObjects,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_0;

extern CL_API_ENTRY cl_int CL_API_CALL
clEnqueueReleaseD3D10ObjectsKHR(
    cl_command_queue commandQueue,
    cl_uint numObjects,
    const cl_mem *memObjects,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_0;

extern CL_API_ENTRY cl_int CL_API_CALL clGetDeviceIDsFromD3D11KHR(
    cl_platform_id platform,
    cl_d3d11_device_source_khr d3dDeviceSource,
    void *d3dObject,
    cl_d3d11_device_set_khr d3dDeviceSet,
    cl_uint numEntries,
    cl_device_id *devices,
    cl_uint *numDevices) CL_API_SUFFIX__VERSION_1_2;

extern CL_API_ENTRY cl_mem CL_API_CALL clCreateFromD3D11BufferKHR(
    cl_context context,
    cl_mem_flags flags,
    ID3D11Buffer *resource,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_2;

extern CL_API_ENTRY cl_mem CL_API_CALL clCreateFromD3D11Texture2DKHR(
    cl_context context,
    cl_mem_flags flags,
    ID3D11Texture2D *resource,
    UINT subresource,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_2;

extern CL_API_ENTRY cl_mem CL_API_CALL clCreateFromD3D11Texture3DKHR(
    cl_context context,
    cl_mem_flags flags,
    ID3D11Texture3D *resource,
    UINT subresource,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_2;

extern CL_API_ENTRY cl_int CL_API_CALL clEnqueueAcquireD3D11ObjectsKHR(
    cl_command_queue commandQueue,
    cl_uint numObjects,
    const cl_mem *memObjects,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_2;

extern CL_API_ENTRY cl_int CL_API_CALL clEnqueueReleaseD3D11ObjectsKHR(
    cl_command_queue commandQueue,
    cl_uint numObjects,
    const cl_mem *memObjects,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_2;
