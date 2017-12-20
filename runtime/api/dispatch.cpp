/*
 * Copyright (c) 2017, Intel Corporation
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

#include "dispatch.h"
#include "api.h"

SDispatchTable icdGlobalDispatchTable =
    {
        clGetPlatformIDs,
        clGetPlatformInfo,
        clGetDeviceIDs,
        clGetDeviceInfo,
        clCreateContext,
        clCreateContextFromType,
        clRetainContext,
        clReleaseContext,
        clGetContextInfo,
        clCreateCommandQueue,
        clRetainCommandQueue,
        clReleaseCommandQueue,
        clGetCommandQueueInfo,
        clSetCommandQueueProperty,
        clCreateBuffer,
        clCreateImage2D,
        clCreateImage3D,
        clRetainMemObject,
        clReleaseMemObject,
        clGetSupportedImageFormats,
        clGetMemObjectInfo,
        clGetImageInfo,
        clCreateSampler,
        clRetainSampler,
        clReleaseSampler,
        clGetSamplerInfo,
        clCreateProgramWithSource,
        clCreateProgramWithBinary,
        clRetainProgram,
        clReleaseProgram,
        clBuildProgram,
        clUnloadCompiler,
        clGetProgramInfo,
        clGetProgramBuildInfo,
        clCreateKernel,
        clCreateKernelsInProgram,
        clRetainKernel,
        clReleaseKernel,
        clSetKernelArg,
        clGetKernelInfo,
        clGetKernelWorkGroupInfo,
        clWaitForEvents,
        clGetEventInfo,
        clRetainEvent,
        clReleaseEvent,
        clGetEventProfilingInfo,
        clFlush,
        clFinish,
        clEnqueueReadBuffer,
        clEnqueueWriteBuffer,
        clEnqueueCopyBuffer,
        clEnqueueReadImage,
        clEnqueueWriteImage,
        clEnqueueCopyImage,
        clEnqueueCopyImageToBuffer,
        clEnqueueCopyBufferToImage,
        clEnqueueMapBuffer,
        clEnqueueMapImage,
        clEnqueueUnmapMemObject,
        clEnqueueNDRangeKernel,
        clEnqueueTask,
        clEnqueueNativeKernel,
        clEnqueueMarker,
        clEnqueueWaitForEvents,
        clEnqueueBarrier,
        clGetExtensionFunctionAddress,

        /* cl_khr_gl_sharing */
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,

        /* cl_khr_d3d10_sharing */
        nullptr, // clGetDeviceIDsFromD3D10KHR,
        nullptr, // clCreateFromD3D10BufferKHR,
        nullptr, // clCreateFromD3D10Texture2DKHR,
        nullptr, // clCreateFromD3D10Texture3DKHR,
        nullptr, // clEnqueueAcquireD3D10ObjectsKHR,
        nullptr, // clEnqueueReleaseD3D10ObjectsKHR,

        /* OpenCL 1.1 */
        clSetEventCallback,
        clCreateSubBuffer,
        clSetMemObjectDestructorCallback,
        clCreateUserEvent,
        clSetUserEventStatus,
        clEnqueueReadBufferRect,
        clEnqueueWriteBufferRect,
        clEnqueueCopyBufferRect,

        /* cl_ext_device_fission */
        nullptr, //clCreateSubDevicesEXT,
        nullptr, //clRetainDeviceEXT,
        nullptr, //clReleaseDeviceEXT,

        /* cl_khr_gl_event */
        nullptr,

        /* OpenCL 1.2 */
        clCreateSubDevices,
        clRetainDevice,
        clReleaseDevice,
        clCreateImage,
        clCreateProgramWithBuiltInKernels,
        clCompileProgram,
        clLinkProgram,
        clUnloadPlatformCompiler,
        clGetKernelArgInfo,
        clEnqueueFillBuffer,
        clEnqueueFillImage,
        clEnqueueMigrateMemObjects,
        clEnqueueMarkerWithWaitList,
        clEnqueueBarrierWithWaitList,
        clGetExtensionFunctionAddressForPlatform,
        nullptr,

        /* cl_khr_d3d11_sharing */
        nullptr, // clGetDeviceIDsFromD3D11KHR,
        nullptr, // clCreateFromD3D11BufferKHR,
        nullptr, // clCreateFromD3D11Texture2DKHR,
        nullptr, // clCreateFromD3D11Texture3DKHR,
        nullptr, // clCreateFromDX9MediaSurfaceKHR,
        nullptr, // clEnqueueAcquireD3D11ObjectsKHR,
        nullptr, // clEnqueueReleaseD3D11ObjectsKHR,

        /* cl_khr_dx9_media_sharing */
        nullptr, // clGetDeviceIDsFromDX9MediaAdapterKHR,
        nullptr, // clEnqueueAcquireDX9MediaSurfacesKHR,
        nullptr, // clEnqueueReleaseDX9MediaSurfacesKHR,

        /* cl_khr_egl_image */
        nullptr, //clCreateFromEGLImageKHR,
        nullptr, //clEnqueueAcquireEGLObjectsKHR,
        nullptr, //clEnqueueReleaseEGLObjectsKHR,

        /* cl_khr_egl_event */
        nullptr, //clCreateEventFromEGLSyncKHR,

        /* OpenCL 2.0 */
        clCreateCommandQueueWithProperties,
        clCreatePipe,
        clGetPipeInfo,
        clSVMAlloc,
        clSVMFree,
        clEnqueueSVMFree,
        clEnqueueSVMMemcpy,
        clEnqueueSVMMemFill,
        clEnqueueSVMMap,
        clEnqueueSVMUnmap,
        clCreateSamplerWithProperties,
        clSetKernelArgSVMPointer,
        clSetKernelExecInfo,
        clGetKernelSubGroupInfoKHR,

        /* OpenCL 2.1 */
        clCloneKernel,
        clCreateProgramWithIL,
        clEnqueueSVMMigrateMem,
        clGetDeviceAndHostTimer,
        clGetHostTimer,
        clGetKernelSubGroupInfo,
        clSetDefaultDeviceCommandQueue,
};
SCRTDispatchTable crtGlobalDispatchTable = {
    clGetKernelArgInfo,

    nullptr, // clGetDeviceIDsFromDX9INTEL,
    nullptr, // clCreateFromDX9MediaSurfaceINTEL,
    nullptr, // clEnqueueAcquireDX9ObjectsINTEL,
    nullptr, // clEnqueueReleaseDX9ObjectsINTEL,
    clGetImageParamsINTEL,
    clCreatePerfCountersCommandQueueINTEL,

    clCreateAcceleratorINTEL,
    clGetAcceleratorInfoINTEL,
    clRetainAcceleratorINTEL,
    clReleaseAcceleratorINTEL,

    nullptr,
    nullptr,

    nullptr,
    nullptr,
    nullptr,
    nullptr,

    nullptr,
    nullptr,
    nullptr,
    nullptr,
    clSetPerformanceConfigurationINTEL};

SEntryPointsTable globalDispatchTable = {&icdGlobalDispatchTable, &crtGlobalDispatchTable};
