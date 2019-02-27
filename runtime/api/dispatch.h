/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "CL/cl.h"
#include "CL/cl_ext.h"
#include "CL/cl_ext_intel.h"
#include "CL/cl_gl.h"
#include "CL/cl_gl_ext.h"
#if defined(_WIN32)
#include <d3d10_1.h>

#include "CL/cl_d3d10.h"
#include "CL/cl_d3d11.h"
#include "CL/cl_dx9_media_sharing.h"
#define CL_DX9_MEDIA_SHARING_INTEL_EXT
#include "runtime/os_interface/windows/windows_wrapper.h"

#include "CL/cl_dx9_media_sharing_intel.h"
#else
#define CL_CONTEXT_D3D10_DEVICE_KHR 0x4014
#define CL_CONTEXT_D3D10_PREFER_SHARED_RESOURCES_KHR 0x402C
#define CL_MEM_D3D10_RESOURCE_KHR 0x4015
typedef cl_uint cl_dx9_device_source_intel;
typedef cl_uint cl_dx9_device_set_intel;
typedef cl_uint cl_dx9_media_adapter_type_khr;
typedef cl_uint cl_dx9_media_adapter_set_khr;
typedef cl_uint cl_d3d10_device_source_khr;
typedef cl_uint cl_d3d10_device_set_khr;
typedef void *IDirect3DSurface9;
typedef void *ID3D10Buffer;
typedef void *ID3D10Texture2D;
typedef void *ID3D10Texture3D;
typedef unsigned int UINT;
typedef cl_uint cl_d3d11_device_source_khr;
typedef cl_uint cl_d3d11_device_set_khr;
typedef void *ID3D11Buffer;
typedef void *ID3D11Texture2D;
typedef void *ID3D11Texture3D;
typedef void *HANDLE;
#endif

typedef cl_bitfield cl_queue_properties_khr;
typedef void(CL_CALLBACK *ctxt_logging_fn)(const char *, const void *, size_t, void *);
typedef void(CL_CALLBACK *prog_logging_fn)(cl_program, void *);
typedef void(CL_CALLBACK *evnt_logging_fn)(cl_event, cl_int, void *);
typedef void(CL_CALLBACK *memobj_logging_fn)(cl_mem, void *);
typedef void(CL_CALLBACK *svmfree_logging_fn)(cl_command_queue, cl_uint, void *[], void *);

/*
*
* function pointer typedefs
*
*/

// Platform APIs
typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clGetPlatformIDs)(
    cl_uint numEntries,
    cl_platform_id *platforms,
    cl_uint *numPlatforms) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clGetPlatformInfo)(
    cl_platform_id platform,
    cl_platform_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet) CL_API_SUFFIX__VERSION_1_0;

// Device APIs
typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clGetDeviceIDs)(
    cl_platform_id platform,
    cl_device_type deviceType,
    cl_uint numEntries,
    cl_device_id *devices,
    cl_uint *numDevices) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clGetDeviceInfo)(
    cl_device_id device,
    cl_device_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet) CL_API_SUFFIX__VERSION_1_0;

// Context APIs
typedef CL_API_ENTRY cl_context(CL_API_CALL *KHRpfn_clCreateContext)(
    const cl_context_properties *properties,
    cl_uint numDevices,
    const cl_device_id *devices,
    ctxt_logging_fn funcNotify,
    void *userData,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_context(CL_API_CALL *KHRpfn_clCreateContextFromType)(
    const cl_context_properties *properties,
    cl_device_type deviceType,
    ctxt_logging_fn funcNotify,
    void *userData,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clRetainContext)(
    cl_context context) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clReleaseContext)(
    cl_context context) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clGetContextInfo)(
    cl_context context,
    cl_context_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet) CL_API_SUFFIX__VERSION_1_0;

// Command Queue APIs
typedef CL_API_ENTRY cl_command_queue(CL_API_CALL *KHRpfn_clCreateCommandQueue)(
    cl_context context,
    cl_device_id device,
    cl_command_queue_properties properties,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clRetainCommandQueue)(
    cl_command_queue commandQueue) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clReleaseCommandQueue)(
    cl_command_queue commandQueue) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clGetCommandQueueInfo)(
    cl_command_queue commandQueue,
    cl_command_queue_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clSetCommandQueueProperty)(
    cl_command_queue commandQueue,
    cl_command_queue_properties properties,
    cl_bool enable,
    cl_command_queue_properties *oldProperties) CL_API_SUFFIX__VERSION_1_0;

// Memory Object APIs
typedef CL_API_ENTRY cl_mem(CL_API_CALL *KHRpfn_clCreateBuffer)(
    cl_context context,
    cl_mem_flags flags,
    size_t size,
    void *hostPtr,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY CL_EXT_PREFIX__VERSION_1_1_DEPRECATED
cl_mem(CL_API_CALL *KHRpfn_clCreateImage2D)(
    cl_context context,
    cl_mem_flags flags,
    const cl_image_format *imageFormat,
    size_t imageWidth,
    size_t imageHeight,
    size_t imageRowPitch,
    void *hostPtr,
    cl_int *errcodeRet) CL_EXT_SUFFIX__VERSION_1_1_DEPRECATED;

typedef CL_API_ENTRY CL_EXT_PREFIX__VERSION_1_1_DEPRECATED
cl_mem(CL_API_CALL *KHRpfn_clCreateImage3D)(
    cl_context context,
    cl_mem_flags flags,
    const cl_image_format *imageFormat,
    size_t imageWidth,
    size_t imageHeight,
    size_t imageDepth,
    size_t imageRowPitch,
    size_t imageSlicePitch,
    void *hostPtr,
    cl_int *errcodeRet) CL_EXT_SUFFIX__VERSION_1_1_DEPRECATED;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clRetainMemObject)(
    cl_mem memobj) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clReleaseMemObject)(
    cl_mem memobj) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clGetSupportedImageFormats)(
    cl_context context,
    cl_mem_flags flags,
    cl_mem_object_type imageType,
    cl_uint numEntries,
    cl_image_format *imageFormats,
    cl_uint *numImageFormats) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clGetMemObjectInfo)(
    cl_mem memobj,
    cl_mem_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clGetImageInfo)(
    cl_mem image,
    cl_image_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet) CL_API_SUFFIX__VERSION_1_0;

// Sampler APIs
typedef CL_API_ENTRY cl_sampler(CL_API_CALL *KHRpfn_clCreateSampler)(
    cl_context context,
    cl_bool normalizedCoords,
    cl_addressing_mode addressingMode,
    cl_filter_mode filterMode,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_sampler(CL_API_CALL *KHRpfn_clCreateSamplerWithProperties)(
    cl_context context,
    const cl_sampler_properties *samplerProperties,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_2_0;

typedef CL_API_ENTRY cl_mem(CL_API_CALL *KHRpfn_clCreatePipe)(
    cl_context context,
    cl_mem_flags flags,
    cl_uint pipePacketSize,
    cl_uint pipeMaxPackets,
    const cl_pipe_properties *properties,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_2_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clGetPipeInfo)(
    cl_mem image,
    cl_pipe_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet) CL_API_SUFFIX__VERSION_2_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clRetainSampler)(
    cl_sampler sampler) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clReleaseSampler)(
    cl_sampler sampler) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clGetSamplerInfo)(
    cl_sampler sampler,
    cl_sampler_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet) CL_API_SUFFIX__VERSION_1_0;

// Program Object APIs
typedef CL_API_ENTRY cl_program(CL_API_CALL *KHRpfn_clCreateProgramWithSource)(
    cl_context context,
    cl_uint count,
    const char **strings,
    const size_t *lengths,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_program(CL_API_CALL *KHRpfn_clCreateProgramWithBinary)(
    cl_context context,
    cl_uint numDevices,
    const cl_device_id *deviceList,
    const size_t *lengths,
    const unsigned char **binaries,
    cl_int *binaryStatus,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clRetainProgram)(
    cl_program program) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clReleaseProgram)(
    cl_program program) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clBuildProgram)(
    cl_program program,
    cl_uint numDevices,
    const cl_device_id *deviceList,
    const char *options,
    prog_logging_fn funcNotify,
    void *userData) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY CL_EXT_PREFIX__VERSION_1_1_DEPRECATED
cl_int(CL_API_CALL *KHRpfn_clUnloadCompiler)() CL_EXT_SUFFIX__VERSION_1_1_DEPRECATED;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clGetProgramInfo)(
    cl_program program,
    cl_program_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clGetProgramBuildInfo)(
    cl_program program,
    cl_device_id device,
    cl_program_build_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet) CL_API_SUFFIX__VERSION_1_0;

// Kernel Object APIs
typedef CL_API_ENTRY cl_kernel(CL_API_CALL *KHRpfn_clCreateKernel)(
    cl_program program,
    const char *kernelName,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clCreateKernelsInProgram)(
    cl_program program,
    cl_uint numKernels,
    cl_kernel *kernels,
    cl_uint *numKernelsRet) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clRetainKernel)(
    cl_kernel kernel) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clReleaseKernel)(
    cl_kernel kernel) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clSetKernelArg)(
    cl_kernel kernel,
    cl_uint argIndex,
    size_t argSize,
    const void *argValue) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clGetKernelInfo)(
    cl_kernel kernel,
    cl_kernel_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clGetKernelWorkGroupInfo)(
    cl_kernel kernel,
    cl_device_id device,
    cl_kernel_work_group_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet) CL_API_SUFFIX__VERSION_1_0;

// Event Object APIs
typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clWaitForEvents)(
    cl_uint numEvents,
    const cl_event *eventList) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clGetEventInfo)(
    cl_event event,
    cl_event_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clRetainEvent)(
    cl_event event) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clReleaseEvent)(
    cl_event event) CL_API_SUFFIX__VERSION_1_0;

// Profiling APIs
typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clGetEventProfilingInfo)(
    cl_event event,
    cl_profiling_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet) CL_API_SUFFIX__VERSION_1_0;

// Flush and Finish APIs
typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clFlush)(
    cl_command_queue commandQueue) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clFinish)(
    cl_command_queue commandQueue) CL_API_SUFFIX__VERSION_1_0;

// Enqueued Commands APIs
typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueReadBuffer)(
    cl_command_queue commandQueue,
    cl_mem buffer,
    cl_bool blockingRead,
    size_t offset,
    size_t cb,
    void *ptr,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueWriteBuffer)(
    cl_command_queue commandQueue,
    cl_mem buffer,
    cl_bool blockingWrite,
    size_t offset,
    size_t cb,
    const void *ptr,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueCopyBuffer)(
    cl_command_queue commandQueue,
    cl_mem srcBuffer,
    cl_mem dstBuffer,
    size_t srcOffset,
    size_t dstOffset,
    size_t cb,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueReadImage)(
    cl_command_queue commandQueue,
    cl_mem image,
    cl_bool blockingRead,
    const size_t *origin,
    const size_t *region,
    size_t rowPitch,
    size_t slicePitch,
    void *ptr,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueWriteImage)(
    cl_command_queue commandQueue,
    cl_mem image,
    cl_bool blockingWrite,
    const size_t *origin,
    const size_t *region,
    size_t inputRowPitch,
    size_t inputSlicePitch,
    const void *ptr,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueCopyImage)(
    cl_command_queue commandQueue,
    cl_mem srcImage,
    cl_mem dstImage,
    const size_t *srcOrigin,
    const size_t *dstOrigin,
    const size_t *region,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueCopyImageToBuffer)(
    cl_command_queue commandQueue,
    cl_mem srcImage,
    cl_mem dstBuffer,
    const size_t *srcOrigin,
    const size_t *region,
    size_t dstOffset,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueCopyBufferToImage)(
    cl_command_queue commandQueue,
    cl_mem srcBuffer,
    cl_mem dstImage,
    size_t srcOffset,
    const size_t *dstOrigin,
    const size_t *region,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY void *(CL_API_CALL *KHRpfn_clEnqueueMapBuffer)(
    cl_command_queue commandQueue,
    cl_mem buffer,
    cl_bool blockingMap,
    cl_map_flags mapFlags,
    size_t offset,
    size_t cb,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event,
    cl_int *errcodeRet)CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY void *(CL_API_CALL *KHRpfn_clEnqueueMapImage)(
    cl_command_queue commandQueue,
    cl_mem image,
    cl_bool blockingMap,
    cl_map_flags mapFlags,
    const size_t *origin,
    const size_t *region,
    size_t *imageRowPitch,
    size_t *imageSlicePitch,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event,
    cl_int *errcodeRet)CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueUnmapMemObject)(
    cl_command_queue commandQueue,
    cl_mem memobj,
    void *mappedPtr,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueNDRangeKernel)(
    cl_command_queue commandQueue,
    cl_kernel kernel,
    cl_uint workDim,
    const size_t *globalWorkOffset,
    const size_t *globalWorkSize,
    const size_t *localWorkSize,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueTask)(
    cl_command_queue commandQueue,
    cl_kernel kernel,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueNativeKernel)(
    cl_command_queue commandQueue,
    void(CL_CALLBACK *userFunc)(void *),
    void *args,
    size_t cbArgs,
    cl_uint numMemObjects,
    const cl_mem *memList,
    const void **argsMemLoc,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY CL_EXT_PREFIX__VERSION_1_1_DEPRECATED
cl_int(CL_API_CALL *KHRpfn_clEnqueueMarker)(
    cl_command_queue commandQueue,
    cl_event *event) CL_EXT_SUFFIX__VERSION_1_1_DEPRECATED;

typedef CL_API_ENTRY CL_EXT_PREFIX__VERSION_1_1_DEPRECATED
cl_int(CL_API_CALL *KHRpfn_clEnqueueWaitForEvents)(
    cl_command_queue commandQueue,
    cl_uint numEvents,
    const cl_event *eventList) CL_EXT_SUFFIX__VERSION_1_1_DEPRECATED;

typedef CL_API_ENTRY CL_EXT_PREFIX__VERSION_1_1_DEPRECATED
cl_int(CL_API_CALL *KHRpfn_clEnqueueBarrier)(
    cl_command_queue commandQueue) CL_EXT_SUFFIX__VERSION_1_1_DEPRECATED;

typedef CL_API_ENTRY CL_EXT_PREFIX__VERSION_1_1_DEPRECATED void *(CL_API_CALL *KHRpfn_clGetExtensionFunctionAddress)(
    const char *functionName)CL_EXT_SUFFIX__VERSION_1_1_DEPRECATED;

typedef CL_API_ENTRY cl_mem(CL_API_CALL *KHRpfn_clCreateFromGLBuffer)(
    cl_context context,
    cl_mem_flags flags,
    cl_GLuint bufobj,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_mem(CL_API_CALL *KHRpfn_clCreateFromGLTexture)(
    cl_context context,
    cl_mem_flags flags,
    cl_GLenum textureTarget,
    cl_GLint miplevel,
    cl_GLuint texture,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY CL_EXT_PREFIX__VERSION_1_1_DEPRECATED cl_mem(CL_API_CALL *KHRpfn_clCreateFromGLTexture2D)(
    cl_context context,
    cl_mem_flags flags,
    cl_GLenum textureTarget,
    cl_GLint miplevel,
    cl_GLuint texture,
    cl_int *errcodeRet) CL_EXT_SUFFIX__VERSION_1_1_DEPRECATED;

typedef CL_API_ENTRY CL_EXT_PREFIX__VERSION_1_1_DEPRECATED cl_mem(CL_API_CALL *KHRpfn_clCreateFromGLTexture3D)(
    cl_context context,
    cl_mem_flags flags,
    cl_GLenum textureTarget,
    cl_GLint miplevel,
    cl_GLuint texture,
    cl_int *errcodeRet) CL_EXT_SUFFIX__VERSION_1_1_DEPRECATED;

typedef CL_API_ENTRY cl_mem(CL_API_CALL *KHRpfn_clCreateFromGLRenderbuffer)(
    cl_context context,
    cl_mem_flags flags,
    cl_GLuint renderbuffer,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clGetGLObjectInfo)(
    cl_mem memobj,
    cl_gl_object_type *glObjectType,
    cl_GLuint *glObjectName) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clGetGLTextureInfo)(
    cl_mem memobj,
    cl_gl_texture_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueAcquireGLObjects)(
    cl_command_queue commandQueue,
    cl_uint numObjects,
    const cl_mem *memObjects,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueReleaseGLObjects)(
    cl_command_queue commandQueue,
    cl_uint numObjects,
    const cl_mem *memObjects,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_0;

/* cl_khr_gl_sharing */
typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clGetGLContextInfoKHR)(
    const cl_context_properties *properties,
    cl_gl_context_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet) CL_API_SUFFIX__VERSION_1_0;

#ifndef _WIN32
typedef void *clGetDeviceIDsFromDX9INTEL_fn;
typedef void *clCreateFromDX9MediaSurfaceINTEL_fn;
typedef void *clEnqueueAcquireDX9ObjectsINTEL_fn;
typedef void *clEnqueueReleaseDX9ObjectsINTEL_fn;
typedef void *clGetDeviceIDsFromDX9MediaAdapterKHR_fn;
typedef void *clCreateFromDX9MediaSurfaceKHR_fn;
typedef void *clEnqueueAcquireDX9MediaSurfacesKHR_fn;
typedef void *clEnqueueReleaseDX9MediaSurfacesKHR_fn;
typedef void *clGetDeviceIDsFromD3D10KHR_fn;
typedef void *clCreateFromD3D10BufferKHR_fn;
typedef void *clCreateFromD3D10Texture2DKHR_fn;
typedef void *clCreateFromD3D10Texture3DKHR_fn;
typedef void *clEnqueueAcquireD3D10ObjectsKHR_fn;
typedef void *clEnqueueReleaseD3D10ObjectsKHR_fn;
typedef void *clGetDeviceIDsFromD3D11KHR_fn;
typedef void *clCreateFromD3D11BufferKHR_fn;
typedef void *clCreateFromD3D11Texture2DKHR_fn;
typedef void *clCreateFromD3D11Texture3DKHR_fn;
typedef void *clEnqueueAcquireD3D11ObjectsKHR_fn;
typedef void *clEnqueueReleaseD3D11ObjectsKHR_fn;
#endif

/* OpenCL 1.1 */

/* cl_kgr_gl_event */
typedef CL_API_ENTRY cl_event(CL_API_CALL *KHRpfn_clCreateEventFromGLsyncKHR)(
    cl_context context,
    cl_GLsync sync,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clSetEventCallback)(
    cl_event event,
    cl_int commandExecCallbackType,
    evnt_logging_fn pfnNotify,
    void *userData) CL_API_SUFFIX__VERSION_1_1;

typedef CL_API_ENTRY cl_mem(CL_API_CALL *KHRpfn_clCreateSubBuffer)(
    cl_mem buffer,
    cl_mem_flags flags,
    cl_buffer_create_type bufferCreateType,
    const void *bufferCreateInfo,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_1;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clSetMemObjectDestructorCallback)(
    cl_mem memobj,
    memobj_logging_fn pfnNotify,
    void *userData) CL_API_SUFFIX__VERSION_1_1;

typedef CL_API_ENTRY cl_event(CL_API_CALL *KHRpfn_clCreateUserEvent)(
    cl_context context,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_1;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clSetUserEventStatus)(
    cl_event event,
    cl_int executionStatus) CL_API_SUFFIX__VERSION_1_1;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueReadBufferRect)(
    cl_command_queue commandQueue,
    cl_mem buffer,
    cl_bool blockingRead,
    const size_t *bufferOffset,
    const size_t *hostOffset,
    const size_t *region,
    size_t bufferRowPitch,
    size_t bufferSlicePitch,
    size_t hostRowPitch,
    size_t hostSlicePitch,
    void *ptr,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_1;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueWriteBufferRect)(
    cl_command_queue commandQueue,
    cl_mem buffer,
    cl_bool blockingRead,
    const size_t *bufferOffset,
    const size_t *hostOffset,
    const size_t *region,
    size_t bufferRowPitch,
    size_t bufferSlicePitch,
    size_t hostRowPitch,
    size_t hostSlicePitch,
    const void *ptr,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_1;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueCopyBufferRect)(
    cl_command_queue commandQueue,
    cl_mem srcBuffer,
    cl_mem dstBuffer,
    const size_t *srcOrigin,
    const size_t *dstOrigin,
    const size_t *region,
    size_t srcRowPitch,
    size_t srcSlicePitch,
    size_t dstRowPitch,
    size_t dstSlicePitch,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_1;

/*OpenCL 1.2*/

typedef CL_API_ENTRY cl_mem(CL_API_CALL *KHRpfn_clCreateImage)(
    cl_context context,
    cl_mem_flags flags,
    const cl_image_format *imageFormat,
    const cl_image_desc *imageDesc,
    void *hostPtr,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clGetKernelArgInfo)(
    cl_kernel kernel,
    cl_uint argIndx,
    cl_kernel_arg_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet) CL_API_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueFillBuffer)(
    cl_command_queue commandQueue,
    cl_mem buffer,
    const void *pattern,
    size_t patternSize,
    size_t offset,
    size_t cb,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueFillImage)(
    cl_command_queue commandQueue,
    cl_mem image,
    const void *fillColor,
    const size_t *origin,
    const size_t *region,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueMigrateMemObjects)(
    cl_command_queue commandQueue,
    cl_uint numMemObjects,
    const cl_mem *memObjects,
    cl_mem_migration_flags flags,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueMarkerWithWaitList)(
    cl_command_queue commandQueue,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueBarrierWithWaitList)(
    cl_command_queue commandQueue,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clCreateSubDevices)(
    cl_device_id inDevice,
    const cl_device_partition_property *properties,
    cl_uint numEntries,
    cl_device_id *outDevices,
    cl_uint *numDevices) CL_API_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clRetainDevice)(
    cl_device_id device) CL_API_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clReleaseDevice)(
    cl_device_id device) CL_API_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_program(CL_API_CALL *KHRpfn_clCreateProgramWithBuiltInKernels)(
    cl_context context,
    cl_uint numDevices,
    const cl_device_id *deviceList,
    const char *kernelNames,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clCompileProgram)(
    cl_program program,
    cl_uint numDevices,
    const cl_device_id *deviceList,
    const char *options,
    cl_uint numInputHeaders,
    const cl_program *inputHeaders,
    const char **headerIncludeNames,
    void(CL_CALLBACK *pfnNotify)(cl_program program, void *userData),
    void *userData) CL_API_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_program(CL_API_CALL *KHRpfn_clLinkProgram)(
    cl_context context,
    cl_uint numDevices,
    const cl_device_id *deviceList,
    const char *options,
    cl_uint numInputPrograms,
    const cl_program *inputPrograms,
    void(CL_CALLBACK *pfnNotify)(cl_program program, void *userData),
    void *userData,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clUnloadPlatformCompiler)(
    cl_platform_id platform) CL_API_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY void *(CL_API_CALL *KHRpfn_clGetExtensionFunctionAddressForPlatform)(
    cl_platform_id platform,
    const char *funcName)CL_API_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clCreateSubDevicesEXT)(
    cl_device_id inDevice,
    const cl_device_partition_property_ext *partitionProperties,
    cl_uint numEntries,
    cl_device_id *outDevices,
    cl_uint *numDevices);

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clRetainDeviceEXT)(
    cl_device_id device) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clReleaseDeviceEXT)(
    cl_device_id device) CL_API_SUFFIX__VERSION_1_0;

/*cl_khr_egl_image*/
typedef void *KHRpfn_clCreateFromEGLImageKHR;
typedef void *KHRpfn_clEnqueueAcquireEGLObjectsKHR;
typedef void *KHRpfn_clEnqueueReleaseEGLObjectsKHR;

/*cl_khr_egl_event*/
typedef void *KHRpfn_clCreateEventFromEGLSyncKHR;

/*OpenCL2.0*/
typedef CL_API_ENTRY cl_command_queue(CL_API_CALL *KHRpfn_clCreateCommandQueueWithProperties)(
    cl_context context,
    cl_device_id device,
    const cl_queue_properties *properties,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_2_0;

typedef CL_API_ENTRY void *(CL_API_CALL *KHRpfn_clSVMAlloc)(
    cl_context context,
    cl_svm_mem_flags flags,
    size_t size,
    cl_uint alignment)CL_API_SUFFIX__VERSION_2_0;

typedef CL_API_ENTRY void(CL_API_CALL *KHRpfn_clSVMFree)(
    cl_context context,
    void *svmPointer) CL_API_SUFFIX__VERSION_2_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueSVMFree)(
    cl_command_queue commandQueue,
    cl_uint numSvmPointers,
    void *svmPointers[],
    void(CL_CALLBACK *pfnFreeFunc)(cl_command_queue queue,
                                   cl_uint numSvmPointers,
                                   void *svmPointers[],
                                   void *userData),
    void *userData,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_2_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueSVMMemcpy)(
    cl_command_queue commandQueue,
    cl_bool blockingCopy,
    void *dstPtr,
    const void *srcPtr,
    size_t size,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_2_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueSVMMemFill)(
    cl_command_queue commandQueue,
    void *svmPtr,
    const void *pattern,
    size_t patternSize,
    size_t size,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_2_0;
typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueSVMMap)(
    cl_command_queue commandQueue,
    cl_bool blockingMap,
    cl_map_flags mapFlags,
    void *svmPtr,
    size_t size,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_2_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueSVMUnmap)(
    cl_command_queue commandQueue,
    void *svmPtr,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_2_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clSetKernelArgSVMPointer)(
    cl_kernel kernel,
    cl_uint argIndex,
    const void *argValue) CL_API_SUFFIX__VERSION_2_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clSetKernelExecInfo)(
    cl_kernel kernel,
    cl_kernel_exec_info paramName,
    size_t paramValueSize,
    const void *paramValue) CL_API_SUFFIX__VERSION_2_0;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clGetKernelSubGroupInfoKHR)(
    cl_kernel kernel,
    cl_device_id device,
    cl_kernel_sub_group_info paramName,
    size_t inputValueSize,
    const void *inputValue,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet) CL_API_SUFFIX__VERSION_2_0;

/*OpenCL2.1*/
typedef CL_API_ENTRY cl_kernel(CL_API_CALL *KHRpfn_clCloneKernel)(
    cl_kernel sourceKernel,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_2_1;

typedef CL_API_ENTRY cl_program(CL_API_CALL *KHRpfn_clCreateProgramWithIL)(
    cl_context context,
    const void *il,
    size_t length,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_2_1;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clEnqueueSVMMigrateMem)(
    cl_command_queue commandQueue,
    cl_uint numSvmPointers,
    const void **svmPointers,
    const size_t *sizes,
    const cl_mem_migration_flags flags,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_2_1;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clGetDeviceAndHostTimer)(
    cl_device_id device,
    cl_ulong *deviceTimestamp,
    cl_ulong *hostTimestamp) CL_API_SUFFIX__VERSION_2_1;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clGetHostTimer)(
    cl_device_id device,
    cl_ulong *hostTimestamp) CL_API_SUFFIX__VERSION_2_1;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clGetKernelSubGroupInfo)(
    cl_kernel kernel,
    cl_device_id device,
    cl_kernel_sub_group_info paramName,
    size_t inputValueSize,
    const void *inputValue,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet) CL_API_SUFFIX__VERSION_2_1;

typedef CL_API_ENTRY cl_int(CL_API_CALL *KHRpfn_clSetDefaultDeviceCommandQueue)(
    cl_context context,
    cl_device_id device,
    cl_command_queue commandQueue) CL_API_SUFFIX__VERSION_2_1;

/* clCreateImage */

typedef CL_API_ENTRY cl_int(CL_API_CALL *INTELpfn_clGetImageParamsINTEL)(
    cl_context context,
    const cl_image_format *imageFormat,
    const cl_image_desc *imageDesc,
    size_t *imageRowPitch,
    size_t *imageSlicePitch) CL_API_SUFFIX__VERSION_1_1;

/* Performance Counter APIs */
typedef CL_API_ENTRY cl_command_queue(CL_API_CALL *INTELpfn_clCreatePerfCountersCommandQueueINTEL)(
    cl_context context,
    cl_device_id device,
    cl_command_queue_properties properties,
    cl_uint configuration,
    cl_int *errcodeRet);

typedef CL_API_ENTRY cl_int(CL_API_CALL *INTELpfn_clSetPerformanceConfigurationINTEL)(
    cl_device_id device,
    cl_uint count,
    cl_uint *offsets,
    cl_uint *values);

/* cl_intel_accelerator */

typedef CL_API_ENTRY cl_accelerator_intel(CL_API_CALL *INTELpfn_clCreateAcceleratorINTEL)(
    cl_context context,
    cl_accelerator_type_intel acceleratorType,
    size_t descriptorSize,
    const void *descriptor,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_int(CL_API_CALL *INTELpfn_clGetAcceleratorInfoINTEL)(
    cl_accelerator_intel accelerator,
    cl_accelerator_info_intel paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet) CL_API_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_int(CL_API_CALL *INTELpfn_clRetainAcceleratorINTEL)(
    cl_accelerator_intel accelerator) CL_API_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_int(CL_API_CALL *INTELpfn_clReleaseAcceleratorINTEL)(
    cl_accelerator_intel accelerator) CL_API_SUFFIX__VERSION_1_2;

/* cl_intel_va_api_media_sharing */
#ifdef LIBVA
#include "CL/cl_va_api_media_sharing_intel.h"
typedef CL_API_ENTRY cl_mem(CL_API_CALL *INTELpfn_clCreateFromVA_APIMediaSurfaceINTEL)(
    cl_context context,
    cl_mem_flags flags,
    VASurfaceID *surface,
    cl_uint plane,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_int(CL_API_CALL *INTELpfn_clGetDeviceIDsFromVA_APIMediaAdapterINTEL)(
    cl_platform_id platform,
    cl_va_api_device_source_intel mediaAdapterType,
    void *mediaAdapter,
    cl_va_api_device_set_intel mediaAdapterSet,
    cl_uint numEntries,
    cl_device_id *devices,
    cl_uint *numDevices) CL_API_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_int(CL_API_CALL *INTELpfn_clEnqueueAcquireVA_APIMediaSurfacesINTEL)(
    cl_command_queue commandQueue,
    cl_uint numObjects,
    const cl_mem *memObjects,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_int(CL_API_CALL *INTELpfn_clEnqueueReleaseVA_APIMediaSurfacesINTEL)(
    cl_command_queue commandQueue,
    cl_uint numObjects,
    const cl_mem *memObjects,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) CL_API_SUFFIX__VERSION_1_2;
#else
typedef void (*INTELpfn_clCreateFromVA_APIMediaSurfaceINTEL)();
typedef void (*INTELpfn_clGetDeviceIDsFromVA_APIMediaAdapterINTEL)();
typedef void (*INTELpfn_clEnqueueAcquireVA_APIMediaSurfacesINTEL)();
typedef void (*INTELpfn_clEnqueueReleaseVA_APIMediaSurfacesINTEL)();
#endif

/*
*
* vendor dispatch table structure
*
* note that the types in the structure KHRicdVendorDispatch mirror the function
* names listed in the string table khrIcdVendorDispatchFunctionNames
*
*/

struct SDispatchTable {
    KHRpfn_clGetPlatformIDs clGetPlatformIDs;
    KHRpfn_clGetPlatformInfo clGetPlatformInfo;
    KHRpfn_clGetDeviceIDs clGetDeviceIDs;
    KHRpfn_clGetDeviceInfo clGetDeviceInfo;
    KHRpfn_clCreateContext clCreateContext;
    KHRpfn_clCreateContextFromType clCreateContextFromType;
    KHRpfn_clRetainContext clRetainContext;
    KHRpfn_clReleaseContext clReleaseContext;
    KHRpfn_clGetContextInfo clGetContextInfo;
    KHRpfn_clCreateCommandQueue clCreateCommandQueue;
    KHRpfn_clRetainCommandQueue clRetainCommandQueue;
    KHRpfn_clReleaseCommandQueue clReleaseCommandQueue;
    KHRpfn_clGetCommandQueueInfo clGetCommandQueueInfo;
    KHRpfn_clSetCommandQueueProperty clSetCommandQueueProperty;
    KHRpfn_clCreateBuffer clCreateBuffer;
    KHRpfn_clCreateImage2D clCreateImage2D;
    KHRpfn_clCreateImage3D clCreateImage3D;
    KHRpfn_clRetainMemObject clRetainMemObject;
    KHRpfn_clReleaseMemObject clReleaseMemObject;
    KHRpfn_clGetSupportedImageFormats clGetSupportedImageFormats;
    KHRpfn_clGetMemObjectInfo clGetMemObjectInfo;
    KHRpfn_clGetImageInfo clGetImageInfo;
    KHRpfn_clCreateSampler clCreateSampler;
    KHRpfn_clRetainSampler clRetainSampler;
    KHRpfn_clReleaseSampler clReleaseSampler;
    KHRpfn_clGetSamplerInfo clGetSamplerInfo;
    KHRpfn_clCreateProgramWithSource clCreateProgramWithSource;
    KHRpfn_clCreateProgramWithBinary clCreateProgramWithBinary;
    KHRpfn_clRetainProgram clRetainProgram;
    KHRpfn_clReleaseProgram clReleaseProgram;
    KHRpfn_clBuildProgram clBuildProgram;
    KHRpfn_clUnloadCompiler clUnloadCompiler;
    KHRpfn_clGetProgramInfo clGetProgramInfo;
    KHRpfn_clGetProgramBuildInfo clGetProgramBuildInfo;
    KHRpfn_clCreateKernel clCreateKernel;
    KHRpfn_clCreateKernelsInProgram clCreateKernelsInProgram;
    KHRpfn_clRetainKernel clRetainKernel;
    KHRpfn_clReleaseKernel clReleaseKernel;
    KHRpfn_clSetKernelArg clSetKernelArg;
    KHRpfn_clGetKernelInfo clGetKernelInfo;
    KHRpfn_clGetKernelWorkGroupInfo clGetKernelWorkGroupInfo;
    KHRpfn_clWaitForEvents clWaitForEvents;
    KHRpfn_clGetEventInfo clGetEventInfo;
    KHRpfn_clRetainEvent clRetainEvent;
    KHRpfn_clReleaseEvent clReleaseEvent;
    KHRpfn_clGetEventProfilingInfo clGetEventProfilingInfo;
    KHRpfn_clFlush clFlush;
    KHRpfn_clFinish clFinish;
    KHRpfn_clEnqueueReadBuffer clEnqueueReadBuffer;
    KHRpfn_clEnqueueWriteBuffer clEnqueueWriteBuffer;
    KHRpfn_clEnqueueCopyBuffer clEnqueueCopyBuffer;
    KHRpfn_clEnqueueReadImage clEnqueueReadImage;
    KHRpfn_clEnqueueWriteImage clEnqueueWriteImage;
    KHRpfn_clEnqueueCopyImage clEnqueueCopyImage;
    KHRpfn_clEnqueueCopyImageToBuffer clEnqueueCopyImageToBuffer;
    KHRpfn_clEnqueueCopyBufferToImage clEnqueueCopyBufferToImage;
    KHRpfn_clEnqueueMapBuffer clEnqueueMapBuffer;
    KHRpfn_clEnqueueMapImage clEnqueueMapImage;
    KHRpfn_clEnqueueUnmapMemObject clEnqueueUnmapMemObject;
    KHRpfn_clEnqueueNDRangeKernel clEnqueueNDRangeKernel;
    KHRpfn_clEnqueueTask clEnqueueTask;
    KHRpfn_clEnqueueNativeKernel clEnqueueNativeKernel;
    KHRpfn_clEnqueueMarker clEnqueueMarker;
    KHRpfn_clEnqueueWaitForEvents clEnqueueWaitForEvents;
    KHRpfn_clEnqueueBarrier clEnqueueBarrier;
    KHRpfn_clGetExtensionFunctionAddress clGetExtensionFunctionAddress;
    KHRpfn_clCreateFromGLBuffer clCreateFromGLBuffer;
    KHRpfn_clCreateFromGLTexture2D clCreateFromGLTexture2D;
    KHRpfn_clCreateFromGLTexture3D clCreateFromGLTexture3D;
    KHRpfn_clCreateFromGLRenderbuffer clCreateFromGLRenderbuffer;
    KHRpfn_clGetGLObjectInfo clGetGLObjectInfo;
    KHRpfn_clGetGLTextureInfo clGetGLTextureInfo;
    KHRpfn_clEnqueueAcquireGLObjects clEnqueueAcquireGLObjects;
    KHRpfn_clEnqueueReleaseGLObjects clEnqueueReleaseGLObjects;
    KHRpfn_clGetGLContextInfoKHR clGetGLContextInfoKHR;

    /* cl_khr_d3d10_sharing */
    clGetDeviceIDsFromD3D10KHR_fn clGetDeviceIDsFromD3D10KHR;
    clCreateFromD3D10BufferKHR_fn clCreateFromD3D10BufferKHR;
    clCreateFromD3D10Texture2DKHR_fn clCreateFromD3D10Texture2DKHR;
    clCreateFromD3D10Texture3DKHR_fn clCreateFromD3D10Texture3DKHR;
    clEnqueueAcquireD3D10ObjectsKHR_fn clEnqueueAcquireD3D10ObjectsKHR;
    clEnqueueReleaseD3D10ObjectsKHR_fn clEnqueueReleaseD3D10ObjectsKHR;

    /* OpenCL 1.1 */
    KHRpfn_clSetEventCallback clSetEventCallback;
    KHRpfn_clCreateSubBuffer clCreateSubBuffer;
    KHRpfn_clSetMemObjectDestructorCallback clSetMemObjectDestructorCallback;
    KHRpfn_clCreateUserEvent clCreateUserEvent;
    KHRpfn_clSetUserEventStatus clSetUserEventStatus;
    KHRpfn_clEnqueueReadBufferRect clEnqueueReadBufferRect;
    KHRpfn_clEnqueueWriteBufferRect clEnqueueWriteBufferRect;
    KHRpfn_clEnqueueCopyBufferRect clEnqueueCopyBufferRect;

    /* cl_ext_device_fission */
    KHRpfn_clCreateSubDevicesEXT clCreateSubDevicesEXT;
    KHRpfn_clRetainDeviceEXT clRetainDeviceEXT;
    KHRpfn_clReleaseDeviceEXT clReleaseDeviceEXT;

    /* cl_khr_gl_event */
    KHRpfn_clCreateEventFromGLsyncKHR clCreateEventFromGLsyncKHR;

    /* OpenCL 1.2 */
    KHRpfn_clCreateSubDevices clCreateSubDevices;
    KHRpfn_clRetainDevice clRetainDevice;
    KHRpfn_clReleaseDevice clReleaseDevice;
    KHRpfn_clCreateImage clCreateImage;
    KHRpfn_clCreateProgramWithBuiltInKernels clCreateProgramWithBuiltInKernels;
    KHRpfn_clCompileProgram clCompileProgram;
    KHRpfn_clLinkProgram clLinkProgram;
    KHRpfn_clUnloadPlatformCompiler clUnloadPlatformCompiler;
    KHRpfn_clGetKernelArgInfo clGetKernelArgInfo;
    KHRpfn_clEnqueueFillBuffer clEnqueueFillBuffer;
    KHRpfn_clEnqueueFillImage clEnqueueFillImage;
    KHRpfn_clEnqueueMigrateMemObjects clEnqueueMigrateMemObjects;
    KHRpfn_clEnqueueMarkerWithWaitList clEnqueueMarkerWithWaitList;
    KHRpfn_clEnqueueBarrierWithWaitList clEnqueueBarrierWithWaitList;
    KHRpfn_clGetExtensionFunctionAddressForPlatform clGetExtensionFunctionAddressForPlatform;
    KHRpfn_clCreateFromGLTexture clCreateFromGLTexture;

    /* cl_khr_d3d11_sharing */
    clGetDeviceIDsFromD3D11KHR_fn clGetDeviceIDsFromD3D11KHR;
    clCreateFromD3D11BufferKHR_fn clCreateFromD3D11BufferKHR;
    clCreateFromD3D11Texture2DKHR_fn clCreateFromD3D11Texture2DKHR;
    clCreateFromD3D11Texture3DKHR_fn clCreateFromD3D11Texture3DKHR;
    clCreateFromDX9MediaSurfaceKHR_fn clCreateFromDX9MediaSurfaceKHR;
    clEnqueueAcquireD3D11ObjectsKHR_fn clEnqueueAcquireD3D11ObjectsKHR;
    clEnqueueReleaseD3D11ObjectsKHR_fn clEnqueueReleaseD3D11ObjectsKHR;

    /* cl_khr_dx9_media_sharing */
    clGetDeviceIDsFromDX9MediaAdapterKHR_fn clGetDeviceIDsFromDX9MediaAdapterKHR;
    clEnqueueAcquireDX9MediaSurfacesKHR_fn clEnqueueAcquireDX9MediaSurfacesKHR;
    clEnqueueReleaseDX9MediaSurfacesKHR_fn clEnqueueReleaseDX9MediaSurfacesKHR;

    /* cl_khr_egl_image */
    KHRpfn_clCreateFromEGLImageKHR clCreateFromEGLImageKHR;
    KHRpfn_clEnqueueAcquireEGLObjectsKHR clEnqueueAcquireEGLObjectsKHR;
    KHRpfn_clEnqueueReleaseEGLObjectsKHR clEnqueueReleaseEGLObjectsKHR;

    /* cl_khr_egl_event */
    KHRpfn_clCreateEventFromEGLSyncKHR clCreateEventFromEGLSyncKHR;

    /* OpenCL 2.0 */
    KHRpfn_clCreateCommandQueueWithProperties clCreateCommandQueueWithProperties;
    KHRpfn_clCreatePipe clCreatePipe;
    KHRpfn_clGetPipeInfo clGetPipeInfo;
    KHRpfn_clSVMAlloc clSVMAlloc;
    KHRpfn_clSVMFree clSVMFree;
    KHRpfn_clEnqueueSVMFree clEnqueueSVMFree;
    KHRpfn_clEnqueueSVMMemcpy clEnqueueSVMMemcpy;
    KHRpfn_clEnqueueSVMMemFill clEnqueueSVMMemFill;
    KHRpfn_clEnqueueSVMMap clEnqueueSVMMap;
    KHRpfn_clEnqueueSVMUnmap clEnqueueSVMUnmap;
    KHRpfn_clCreateSamplerWithProperties clCreateSamplerWithProperties;
    KHRpfn_clSetKernelArgSVMPointer clSetKernelArgSVMPointer;
    KHRpfn_clSetKernelExecInfo clSetKernelExecInfo;

    KHRpfn_clGetKernelSubGroupInfoKHR clGetKernelSubGroupInfoKHR;

    /* OpenCL 2.1 */
    KHRpfn_clCloneKernel clCloneKernel;
    KHRpfn_clCreateProgramWithIL clCreateProgramWithIL;
    KHRpfn_clEnqueueSVMMigrateMem clEnqueueSVMMigrateMem;
    KHRpfn_clGetDeviceAndHostTimer clGetDeviceAndHostTimer;
    KHRpfn_clGetHostTimer clGetHostTimer;
    KHRpfn_clGetKernelSubGroupInfo clGetKernelSubGroupInfo;
    KHRpfn_clSetDefaultDeviceCommandQueue clSetDefaultDeviceCommandQueue;
};

struct SCRTDispatchTable {
    // Support CRT entry point
    KHRpfn_clGetKernelArgInfo clGetKernelArgInfo;

    clGetDeviceIDsFromDX9INTEL_fn clGetDeviceIDsFromDX9INTEL;
    clCreateFromDX9MediaSurfaceINTEL_fn clCreateFromDX9MediaSurfaceINTEL;
    clEnqueueAcquireDX9ObjectsINTEL_fn clEnqueueAcquireDX9ObjectsINTEL;
    clEnqueueReleaseDX9ObjectsINTEL_fn clEnqueueReleaseDX9ObjectsINTEL;

    INTELpfn_clGetImageParamsINTEL clGetImageParamsINTEL;
    // API to expose the Performance Counters to applications
    INTELpfn_clCreatePerfCountersCommandQueueINTEL clCreatePerfCountersCommandQueueINTEL;

    // Video Analytics Accelerator
    INTELpfn_clCreateAcceleratorINTEL clCreateAcceleratorINTEL;
    INTELpfn_clGetAcceleratorInfoINTEL clGetAcceleratorInfoINTEL;
    INTELpfn_clRetainAcceleratorINTEL clRetainAcceleratorINTEL;
    INTELpfn_clReleaseAcceleratorINTEL clReleaseAcceleratorINTEL;

    void *placeholder12;
    void *placeholder13;

// VAMedia sharing extension
#ifdef LIBVA
    INTELpfn_clCreateFromVA_APIMediaSurfaceINTEL clCreateFromVA_APIMediaSurfaceINTEL;
    INTELpfn_clGetDeviceIDsFromVA_APIMediaAdapterINTEL clGetDeviceIDsFromVA_APIMediaAdapterINTEL;
    INTELpfn_clEnqueueReleaseVA_APIMediaSurfacesINTEL clEnqueueReleaseVA_APIMediaSurfacesINTEL;
    INTELpfn_clEnqueueAcquireVA_APIMediaSurfacesINTEL clEnqueueAcquireVA_APIMediaSurfacesINTEL;
#else
    void *placeholder14;
    void *placeholder15;
    void *placeholder16;
    void *placeholder17;
#endif

    void *placeholder18;
    void *placeholder19;
    void *placeholder20;
    void *placeholder21;
    // OCL Performance Counters configuration
    INTELpfn_clSetPerformanceConfigurationINTEL clSetPerformanceConfigurationINTEL;
};

extern SDispatchTable icdGlobalDispatchTable;
extern SCRTDispatchTable crtGlobalDispatchTable;

struct SEntryPointsTable {
    SDispatchTable *icdDispatch;
    SCRTDispatchTable *crtDispatch;
};

struct SEntryPointsTableData {
    SDispatchTable icdDispatch;
    SCRTDispatchTable crtDispatch;
};

extern SEntryPointsTable globalDispatchTable;
