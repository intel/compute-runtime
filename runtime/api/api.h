/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "public/cl_ext_private.h"
#include "runtime/api/dispatch.h"

#include "CL/cl.h"
#include "CL/cl_gl.h"

#ifdef __cplusplus
extern "C" {
#endif

cl_int CL_API_CALL clGetPlatformIDs(
    cl_uint numEntries,
    cl_platform_id *platforms,
    cl_uint *numPlatforms);

cl_int CL_API_CALL clGetPlatformInfo(
    cl_platform_id platform,
    cl_platform_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet);

cl_int CL_API_CALL clGetDeviceIDs(
    cl_platform_id platform,
    cl_device_type deviceType,
    cl_uint numEntries,
    cl_device_id *devices,
    cl_uint *numDevices);

cl_int CL_API_CALL clGetDeviceInfo(
    cl_device_id device,
    cl_device_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet);

cl_int CL_API_CALL clCreateSubDevices(
    cl_device_id inDevice,
    const cl_device_partition_property *properties,
    cl_uint numDevices,
    cl_device_id *outDevices,
    cl_uint *numDevicesRet);

cl_int CL_API_CALL clRetainDevice(
    cl_device_id device);

cl_int CL_API_CALL clReleaseDevice(
    cl_device_id device);

cl_context CL_API_CALL clCreateContext(
    const cl_context_properties *properties,
    cl_uint numDevices,
    const cl_device_id *devices,
    void(CL_CALLBACK *funcNotify)(const char *, const void *, size_t, void *),
    void *userData,
    cl_int *errcodeRet);

cl_context CL_API_CALL clCreateContextFromType(
    const cl_context_properties *properties,
    cl_device_type deviceType,
    void(CL_CALLBACK *funcNotify)(const char *, const void *, size_t, void *),
    void *userData,
    cl_int *errcodeRet);

cl_int CL_API_CALL clRetainContext(
    cl_context context);

cl_int CL_API_CALL clReleaseContext(
    cl_context context);

cl_int CL_API_CALL clGetContextInfo(
    cl_context context,
    cl_context_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet);

cl_int CL_API_CALL clGetGLContextInfoKHR(
    const cl_context_properties *properties,
    cl_gl_context_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet);

cl_command_queue CL_API_CALL clCreateCommandQueue(
    cl_context context,
    cl_device_id device,
    cl_command_queue_properties properties,
    cl_int *errcodeRet);

cl_int CL_API_CALL clRetainCommandQueue(
    cl_command_queue commandQueue);

cl_int CL_API_CALL clReleaseCommandQueue(
    cl_command_queue commandQueue);

cl_int CL_API_CALL clGetCommandQueueInfo(
    cl_command_queue commandQueue,
    cl_command_queue_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet);

// deprecated OpenCL 1.0
cl_int CL_API_CALL clSetCommandQueueProperty(
    cl_command_queue commandQueue,
    cl_command_queue_properties properties,
    cl_bool enable,
    cl_command_queue_properties *oldProperties);

cl_mem CL_API_CALL clCreateBuffer(
    cl_context context,
    cl_mem_flags flags,
    size_t size,
    void *hostPtr,
    cl_int *errcodeRet);

cl_mem CL_API_CALL clCreateBufferWithPropertiesINTEL(
    cl_context context,
    const cl_mem_properties_intel *properties,
    size_t size,
    void *hostPtr,
    cl_int *errcodeRet);

cl_mem CL_API_CALL clCreateSubBuffer(
    cl_mem buffer,
    cl_mem_flags flags,
    cl_buffer_create_type bufferCreateType,
    const void *bufferCreateInfo,
    cl_int *errcodeRet);

cl_mem CL_API_CALL clCreateImage(
    cl_context context,
    cl_mem_flags flags,
    const cl_image_format *imageFormat,
    const cl_image_desc *imageDesc,
    void *hostPtr,
    cl_int *errcodeRet);

// deprecated OpenCL 1.1
cl_mem CL_API_CALL clCreateImage2D(
    cl_context context,
    cl_mem_flags flags,
    const cl_image_format *imageFormat,
    size_t imageWidth,
    size_t imageHeight,
    size_t imageRowPitch,
    void *hostPtr,
    cl_int *errcodeRet);

// deprecated OpenCL 1.1
cl_mem CL_API_CALL clCreateImage3D(
    cl_context context,
    cl_mem_flags flags,
    const cl_image_format *imageFormat,
    size_t imageWidth,
    size_t imageHeight,
    size_t imageDepth,
    size_t imageRowPitch,
    size_t imageSlicePitch,
    void *hostPtr,
    cl_int *errcodeRet);

cl_int CL_API_CALL clRetainMemObject(
    cl_mem memobj);

cl_int CL_API_CALL clReleaseMemObject(
    cl_mem memobj);

cl_int CL_API_CALL clGetSupportedImageFormats(
    cl_context context,
    cl_mem_flags flags,
    cl_mem_object_type imageType,
    cl_uint numEntries,
    cl_image_format *imageFormats,
    cl_uint *numImageFormats);

cl_int CL_API_CALL clGetMemObjectInfo(
    cl_mem memobj,
    cl_mem_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet);

cl_int CL_API_CALL clGetImageInfo(
    cl_mem image,
    cl_image_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet);

cl_int CL_API_CALL clGetImageParamsINTEL(
    cl_context context,
    const cl_image_format *imageFormat,
    const cl_image_desc *imageDesc,
    size_t *imageRowPitch,
    size_t *imageSlicePitch);

cl_int CL_API_CALL clSetMemObjectDestructorCallback(
    cl_mem memobj,
    void(CL_CALLBACK *funcNotify)(cl_mem, void *),
    void *userData);

cl_sampler CL_API_CALL clCreateSampler(
    cl_context context,
    cl_bool normalizedCoords,
    cl_addressing_mode addressingMode,
    cl_filter_mode filterMode,
    cl_int *errcodeRet);

cl_int CL_API_CALL clRetainSampler(
    cl_sampler sampler);

cl_int CL_API_CALL clReleaseSampler(
    cl_sampler sampler);

cl_int CL_API_CALL clGetSamplerInfo(
    cl_sampler sampler,
    cl_sampler_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet);

cl_program CL_API_CALL clCreateProgramWithSource(
    cl_context context,
    cl_uint count,
    const char **strings,
    const size_t *lengths,
    cl_int *errcodeRet);

cl_program CL_API_CALL clCreateProgramWithBinary(
    cl_context context,
    cl_uint numDevices,
    const cl_device_id *deviceList,
    const size_t *lengths,
    const unsigned char **binaries,
    cl_int *binaryStatus,
    cl_int *errcodeRet);

cl_program CL_API_CALL clCreateProgramWithIL(cl_context context,
                                             const void *il,
                                             size_t length,
                                             cl_int *errcodeRet);

cl_program CL_API_CALL clCreateProgramWithBuiltInKernels(
    cl_context context,
    cl_uint numDevices,
    const cl_device_id *deviceList,
    const char *kernelNames,
    cl_int *errcodeRet);

cl_int CL_API_CALL clRetainProgram(
    cl_program program);

cl_int CL_API_CALL clReleaseProgram(
    cl_program program);

cl_int CL_API_CALL clBuildProgram(
    cl_program program,
    cl_uint numDevices,
    const cl_device_id *deviceList,
    const char *options,
    void(CL_CALLBACK *funcNotify)(cl_program program, void *userData),
    void *userData);

cl_int CL_API_CALL clCompileProgram(
    cl_program program,
    cl_uint numDevices,
    const cl_device_id *deviceList,
    const char *options,
    cl_uint numInputHeaders,
    const cl_program *inputHeaders,
    const char **headerIncludeNames,
    void(CL_CALLBACK *funcNotify)(cl_program program, void *userData),
    void *userData);

cl_program CL_API_CALL clLinkProgram(
    cl_context context,
    cl_uint numDevices,
    const cl_device_id *deviceList,
    const char *options,
    cl_uint numInputPrograms,
    const cl_program *inputPrograms,
    void(CL_CALLBACK *funcNotify)(cl_program program, void *userData),
    void *userData,
    cl_int *errcodeRet);

cl_int CL_API_CALL clUnloadPlatformCompiler(
    cl_platform_id platform);

// deprecated OpenCL 1.1
cl_int CL_API_CALL clUnloadCompiler(void);

cl_int CL_API_CALL clGetProgramInfo(
    cl_program program,
    cl_program_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet);

cl_int CL_API_CALL clGetProgramBuildInfo(
    cl_program program,
    cl_device_id device,
    cl_program_build_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet);

cl_kernel CL_API_CALL clCreateKernel(
    cl_program program,
    const char *kernelName,
    cl_int *errcodeRet);

cl_int CL_API_CALL clCreateKernelsInProgram(
    cl_program program,
    cl_uint numKernels,
    cl_kernel *kernels,
    cl_uint *numKernelsRet);

cl_int CL_API_CALL clRetainKernel(
    cl_kernel kernel);

cl_int CL_API_CALL clReleaseKernel(
    cl_kernel kernel);

cl_int CL_API_CALL clSetKernelArg(
    cl_kernel kernel,
    cl_uint argIndex,
    size_t argSize,
    const void *argValue);

cl_int CL_API_CALL clGetKernelInfo(
    cl_kernel kernel,
    cl_kernel_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet);

cl_int CL_API_CALL clGetKernelArgInfo(
    cl_kernel kernel,
    cl_uint argIndx,
    cl_kernel_arg_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet);

cl_int CL_API_CALL clGetKernelWorkGroupInfo(
    cl_kernel kernel,
    cl_device_id device,
    cl_kernel_work_group_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet);

cl_int CL_API_CALL clWaitForEvents(
    cl_uint numEvents,
    const cl_event *eventList);

cl_int CL_API_CALL clGetEventInfo(
    cl_event event,
    cl_event_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet);

cl_event CL_API_CALL clCreateUserEvent(
    cl_context context,
    cl_int *errcodeRet);

cl_int CL_API_CALL clRetainEvent(
    cl_event event);

cl_int CL_API_CALL clReleaseEvent(
    cl_event event);

cl_int CL_API_CALL clSetUserEventStatus(
    cl_event event,
    cl_int executionStatus);

cl_int CL_API_CALL clSetEventCallback(
    cl_event event,
    cl_int commandExecCallbackType,
    void(CL_CALLBACK *funcNotify)(cl_event, cl_int, void *),
    void *userData);

cl_int CL_API_CALL clGetEventProfilingInfo(
    cl_event event,
    cl_profiling_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet);

cl_int CL_API_CALL clFlush(
    cl_command_queue commandQueue);

cl_int CL_API_CALL clFinish(
    cl_command_queue commandQueue);

cl_int CL_API_CALL clEnqueueReadBuffer(
    cl_command_queue commandQueue,
    cl_mem buffer,
    cl_bool blockingRead,
    size_t offset,
    size_t cb,
    void *ptr,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event);

cl_int CL_API_CALL clEnqueueReadBufferRect(
    cl_command_queue commandQueue,
    cl_mem buffer,
    cl_bool blockingRead,
    const size_t *bufferOrigin,
    const size_t *hostOrigin,
    const size_t *region,
    size_t bufferRowPitch,
    size_t bufferSlicePitch,
    size_t hostRowPitch,
    size_t hostSlicePitch,
    void *ptr,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event);

cl_int CL_API_CALL clEnqueueWriteBuffer(
    cl_command_queue commandQueue,
    cl_mem buffer,
    cl_bool blockingWrite,
    size_t offset,
    size_t cb,
    const void *ptr,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event);

cl_int CL_API_CALL clEnqueueWriteBufferRect(
    cl_command_queue commandQueue,
    cl_mem buffer,
    cl_bool blockingWrite,
    const size_t *bufferOrigin,
    const size_t *hostOrigin,
    const size_t *region,
    size_t bufferRowPitch,
    size_t bufferSlicePitch,
    size_t hostRowPitch,
    size_t hostSlicePitch,
    const void *ptr,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event);

cl_int CL_API_CALL clEnqueueFillBuffer(
    cl_command_queue commandQueue,
    cl_mem buffer,
    const void *pattern,
    size_t patternSize,
    size_t offset,
    size_t size,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event);

cl_int CL_API_CALL clEnqueueCopyBuffer(
    cl_command_queue commandQueue,
    cl_mem srcBuffer,
    cl_mem dstBuffer,
    size_t srcOffset,
    size_t dstOffset,
    size_t cb,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event);

cl_int CL_API_CALL clEnqueueCopyBufferRect(
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
    cl_event *event);

cl_int CL_API_CALL clEnqueueReadImage(
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
    cl_event *event);

cl_int CL_API_CALL clEnqueueWriteImage(
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
    cl_event *event);

cl_int CL_API_CALL clEnqueueFillImage(
    cl_command_queue commandQueue,
    cl_mem image,
    const void *fillColor,
    const size_t *origin,
    const size_t *region,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event);

cl_int CL_API_CALL clEnqueueCopyImage(
    cl_command_queue commandQueue,
    cl_mem srcImage,
    cl_mem dstImage,
    const size_t *srcOrigin,
    const size_t *dstOrigin,
    const size_t *region,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event);

cl_int CL_API_CALL clEnqueueCopyImageToBuffer(
    cl_command_queue commandQueue,
    cl_mem srcImage,
    cl_mem dstBuffer,
    const size_t *srcOrigin,
    const size_t *region,
    size_t dstOffset,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event);

cl_int CL_API_CALL clEnqueueCopyBufferToImage(
    cl_command_queue commandQueue,
    cl_mem srcBuffer,
    cl_mem dstImage,
    size_t srcOffset,
    const size_t *dstOrigin,
    const size_t *region,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event);

void *CL_API_CALL clEnqueueMapBuffer(
    cl_command_queue commandQueue,
    cl_mem buffer,
    cl_bool blockingMap,
    cl_map_flags mapFlags,
    size_t offset,
    size_t cb,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event,
    cl_int *errcodeRet);

void *CL_API_CALL clEnqueueMapImage(
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
    cl_int *errcodeRet);

cl_int CL_API_CALL clEnqueueUnmapMemObject(
    cl_command_queue commandQueue,
    cl_mem memobj,
    void *mappedPtr,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event);

cl_int CL_API_CALL clEnqueueMigrateMemObjects(
    cl_command_queue commandQueue,
    cl_uint numMemObjects,
    const cl_mem *memObjects,
    cl_mem_migration_flags flags,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event);

cl_int CL_API_CALL clEnqueueNDRangeKernel(
    cl_command_queue commandQueue,
    cl_kernel kernel,
    cl_uint workDim,
    const size_t *globalWorkOffset,
    const size_t *globalWorkSize,
    const size_t *localWorkSize,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event);

cl_int CL_API_CALL clEnqueueTask(
    cl_command_queue commandQueue,
    cl_kernel kernel,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event);

cl_int CL_API_CALL clEnqueueNativeKernel(
    cl_command_queue commandQueue,
    void(CL_CALLBACK *userFunc)(void *),
    void *args,
    size_t cbArgs,
    cl_uint numMemObjects,
    const cl_mem *memList,
    const void **argsMemLoc,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event);

// deprecated OpenCL 1.1
cl_int CL_API_CALL clEnqueueMarker(
    cl_command_queue commandQueue,
    cl_event *event);

// deprecated OpenCL 1.1
cl_int CL_API_CALL clEnqueueWaitForEvents(
    cl_command_queue commandQueue,
    cl_uint numEvents,
    const cl_event *eventList);

// deprecated OpenCL 1.1
cl_int CL_API_CALL clEnqueueBarrier(
    cl_command_queue commandQueue);

cl_int CL_API_CALL clEnqueueMarkerWithWaitList(
    cl_command_queue commandQueue,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event);

cl_int CL_API_CALL clEnqueueBarrierWithWaitList(
    cl_command_queue commandQueue,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event);

// deprecated OpenCL 1.1
void *CL_API_CALL clGetExtensionFunctionAddress(
    const char *funcName);

void *CL_API_CALL clGetExtensionFunctionAddressForPlatform(
    cl_platform_id platform,
    const char *funcName);

// CL-GL Sharing

cl_mem CL_API_CALL clCreateFromGLBuffer(
    cl_context context,
    cl_mem_flags flags,
    cl_GLuint bufobj,
    int *errcodeRet);

// OpenCL 1.2
cl_mem CL_API_CALL clCreateFromGLTexture(
    cl_context context,
    cl_mem_flags flags,
    cl_GLenum target,
    cl_GLint miplevel,
    cl_GLuint texture,
    cl_int *errcodeRet);

// deprecated OpenCL 1.1
cl_mem CL_API_CALL clCreateFromGLTexture2D(
    cl_context context,
    cl_mem_flags flags,
    cl_GLenum target,
    cl_GLint miplevel,
    cl_GLuint texture,
    cl_int *errcodeRet);

// deprecated OpenCL 1.1
cl_mem CL_API_CALL clCreateFromGLTexture3D(
    cl_context context,
    cl_mem_flags flags,
    cl_GLenum target,
    cl_GLint miplevel,
    cl_GLuint texture,
    cl_int *errcodeRet);

cl_mem CL_API_CALL clCreateFromGLRenderbuffer(
    cl_context context,
    cl_mem_flags flags,
    cl_GLuint renderbuffer,
    cl_int *errcodeRet);

cl_int CL_API_CALL clGetGLObjectInfo(
    cl_mem memobj,
    cl_gl_object_type *glObjectType,
    cl_GLuint *glObjectName);

cl_int CL_API_CALL clGetGLTextureInfo(
    cl_mem memobj,
    cl_gl_texture_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet);

cl_int CL_API_CALL clEnqueueAcquireGLObjects(
    cl_command_queue commandQueue,
    cl_uint numObjects,
    const cl_mem *memObjects,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event);

cl_int CL_API_CALL clEnqueueReleaseGLObjects(
    cl_command_queue commandQueue,
    cl_uint numObjects,
    const cl_mem *memObjects,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event);

// OpenCL 2.0

void *CL_API_CALL clSVMAlloc(
    cl_context context,
    cl_svm_mem_flags flags,
    size_t size,
    cl_uint alignment);

void CL_API_CALL clSVMFree(
    cl_context context,
    void *svmPointer);

cl_int CL_API_CALL clEnqueueSVMFree(
    cl_command_queue commandQueue,
    cl_uint numSvmPointers,
    void *svmPointers[],
    void(CL_CALLBACK *pfnFreeFunc)(
        cl_command_queue queue,
        cl_uint numSvmPointers,
        void *svmPointers[],
        void *userData),
    void *userData,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event);

cl_int CL_API_CALL clEnqueueSVMMemcpy(
    cl_command_queue commandQueue,
    cl_bool blockingCopy,
    void *dstPtr,
    const void *srcPtr,
    size_t size,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event);

cl_int CL_API_CALL clEnqueueSVMMemFill(
    cl_command_queue commandQueue,
    void *svmPtr,
    const void *pattern,
    size_t patternSize,
    size_t size,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event);

cl_int CL_API_CALL clEnqueueSVMMap(
    cl_command_queue commandQueue,
    cl_bool blockingMap,
    cl_map_flags mapFlags,
    void *svmPtr,
    size_t size,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event);

cl_int CL_API_CALL clEnqueueSVMUnmap(
    cl_command_queue commandQueue,
    void *svmPtr,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event);

cl_int CL_API_CALL clSetKernelArgSVMPointer(
    cl_kernel kernel,
    cl_uint argIndex,
    const void *argValue);

cl_int CL_API_CALL clSetKernelExecInfo(
    cl_kernel kernel,
    cl_kernel_exec_info paramName,
    size_t paramValueSize,
    const void *paramValue);

cl_mem CL_API_CALL clCreatePipe(
    cl_context context,
    cl_mem_flags flags,
    cl_uint pipePacketSize,
    cl_uint pipeMaxPackets,
    const cl_pipe_properties *properties,
    cl_int *errcodeRet);

cl_int CL_API_CALL clGetPipeInfo(
    cl_mem pipe,
    cl_pipe_info paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet);

cl_command_queue CL_API_CALL clCreateCommandQueueWithProperties(
    cl_context context,
    cl_device_id device,
    const cl_queue_properties *properties,
    cl_int *errcodeRet);

cl_command_queue CL_API_CALL clCreateCommandQueueWithPropertiesKHR(
    cl_context context,
    cl_device_id device,
    const cl_queue_properties_khr *properties,
    cl_int *errcodeRet);

cl_sampler CL_API_CALL clCreateSamplerWithProperties(
    cl_context context,
    const cl_sampler_properties *samplerProperties,
    cl_int *errcodeRet);

cl_int CL_API_CALL clEnqueueVerifyMemoryINTEL(
    cl_command_queue commandQueue,
    const void *allocationPtr,
    const void *expectedData,
    size_t sizeOfComparison,
    cl_uint comparisonMode);

cl_int CL_API_CALL clAddCommentINTEL(cl_platform_id platform, const char *comment);

// OpenCL 2.1

cl_int CL_API_CALL clGetDeviceAndHostTimer(cl_device_id device,
                                           cl_ulong *deviceTimestamp,
                                           cl_ulong *hostTimestamp);

cl_int CL_API_CALL clGetHostTimer(cl_device_id device,
                                  cl_ulong *hostTimestamp);

cl_int CL_API_CALL clGetKernelSubGroupInfo(cl_kernel kernel,
                                           cl_device_id device,
                                           cl_kernel_sub_group_info paramName,
                                           size_t inputValueSize,
                                           const void *inputValue,
                                           size_t paramValueSize,
                                           void *paramValue,
                                           size_t *paramValueSizeRet);

cl_int CL_API_CALL clSetDefaultDeviceCommandQueue(cl_context context,
                                                  cl_device_id device,
                                                  cl_command_queue commandQueue);

cl_int CL_API_CALL clEnqueueSVMMigrateMem(cl_command_queue commandQueue,
                                          cl_uint numSvmPointers,
                                          const void **svmPointers,
                                          const size_t *sizes,
                                          const cl_mem_migration_flags flags,
                                          cl_uint numEventsInWaitList,
                                          const cl_event *eventWaitList,
                                          cl_event *event);

cl_kernel CL_API_CALL clCloneKernel(cl_kernel sourceKernel,
                                    cl_int *errcodeRet);

extern CL_API_ENTRY cl_command_queue CL_API_CALL
clCreatePerfCountersCommandQueueINTEL(
    cl_context context,
    cl_device_id device,
    cl_command_queue_properties properties,
    cl_uint configuration,
    cl_int *errcodeRet);

extern CL_API_ENTRY cl_int CL_API_CALL
clSetPerformanceConfigurationINTEL(
    cl_device_id device,
    cl_uint count,
    cl_uint *offsets,
    cl_uint *values);

extern CL_API_ENTRY cl_event CL_API_CALL
clCreateEventFromGLsyncKHR(
    cl_context context,
    cl_GLsync sync,
    cl_int *errcodeRet) CL_EXT_SUFFIX__VERSION_1_2;

extern CL_API_ENTRY cl_program CL_API_CALL clCreateProgramWithILKHR(
    cl_context context,
    const void *il,
    size_t length,
    cl_int *errcodeRet) CL_API_SUFFIX__VERSION_1_2;
}
