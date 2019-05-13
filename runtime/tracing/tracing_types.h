/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "CL/cl.h"
#include "CL/cl_gl.h"

struct _cl_tracing_handle;
typedef _cl_tracing_handle *cl_tracing_handle;

typedef enum _cl_callback_site {
    CL_CALLBACK_SITE_ENTER = 0,
    CL_CALLBACK_SITE_EXIT = 1
} cl_callback_site;

typedef struct _cl_callback_data {
    cl_callback_site site;
    cl_uint correlationId;
    cl_ulong *correlationData;
    const char *functionName;
    const void *functionParams;
    void *functionReturnValue;
} cl_callback_data;

typedef enum _cl_function_id {
    CL_FUNCTION_clBuildProgram = 0,
    CL_FUNCTION_clCloneKernel = 1,
    CL_FUNCTION_clCompileProgram = 2,
    CL_FUNCTION_clCreateBuffer = 3,
    CL_FUNCTION_clCreateCommandQueue = 4,
    CL_FUNCTION_clCreateCommandQueueWithProperties = 5,
    CL_FUNCTION_clCreateContext = 6,
    CL_FUNCTION_clCreateContextFromType = 7,
    CL_FUNCTION_clCreateFromGLBuffer = 8,
    CL_FUNCTION_clCreateFromGLRenderbuffer = 9,
    CL_FUNCTION_clCreateFromGLTexture = 10,
    CL_FUNCTION_clCreateFromGLTexture2D = 11,
    CL_FUNCTION_clCreateFromGLTexture3D = 12,
    CL_FUNCTION_clCreateImage = 13,
    CL_FUNCTION_clCreateImage2D = 14,
    CL_FUNCTION_clCreateImage3D = 15,
    CL_FUNCTION_clCreateKernel = 16,
    CL_FUNCTION_clCreateKernelsInProgram = 17,
    CL_FUNCTION_clCreatePipe = 18,
    CL_FUNCTION_clCreateProgramWithBinary = 19,
    CL_FUNCTION_clCreateProgramWithBuiltInKernels = 20,
    CL_FUNCTION_clCreateProgramWithIL = 21,
    CL_FUNCTION_clCreateProgramWithSource = 22,
    CL_FUNCTION_clCreateSampler = 23,
    CL_FUNCTION_clCreateSamplerWithProperties = 24,
    CL_FUNCTION_clCreateSubBuffer = 25,
    CL_FUNCTION_clCreateSubDevices = 26,
    CL_FUNCTION_clCreateUserEvent = 27,
    CL_FUNCTION_clEnqueueAcquireGLObjects = 28,
    CL_FUNCTION_clEnqueueBarrier = 29,
    CL_FUNCTION_clEnqueueBarrierWithWaitList = 30,
    CL_FUNCTION_clEnqueueCopyBuffer = 31,
    CL_FUNCTION_clEnqueueCopyBufferRect = 32,
    CL_FUNCTION_clEnqueueCopyBufferToImage = 33,
    CL_FUNCTION_clEnqueueCopyImage = 34,
    CL_FUNCTION_clEnqueueCopyImageToBuffer = 35,
    CL_FUNCTION_clEnqueueFillBuffer = 36,
    CL_FUNCTION_clEnqueueFillImage = 37,
    CL_FUNCTION_clEnqueueMapBuffer = 38,
    CL_FUNCTION_clEnqueueMapImage = 39,
    CL_FUNCTION_clEnqueueMarker = 40,
    CL_FUNCTION_clEnqueueMarkerWithWaitList = 41,
    CL_FUNCTION_clEnqueueMigrateMemObjects = 42,
    CL_FUNCTION_clEnqueueNDRangeKernel = 43,
    CL_FUNCTION_clEnqueueNativeKernel = 44,
    CL_FUNCTION_clEnqueueReadBuffer = 45,
    CL_FUNCTION_clEnqueueReadBufferRect = 46,
    CL_FUNCTION_clEnqueueReadImage = 47,
    CL_FUNCTION_clEnqueueReleaseGLObjects = 48,
    CL_FUNCTION_clEnqueueSVMFree = 49,
    CL_FUNCTION_clEnqueueSVMMap = 50,
    CL_FUNCTION_clEnqueueSVMMemFill = 51,
    CL_FUNCTION_clEnqueueSVMMemcpy = 52,
    CL_FUNCTION_clEnqueueSVMMigrateMem = 53,
    CL_FUNCTION_clEnqueueSVMUnmap = 54,
    CL_FUNCTION_clEnqueueTask = 55,
    CL_FUNCTION_clEnqueueUnmapMemObject = 56,
    CL_FUNCTION_clEnqueueWaitForEvents = 57,
    CL_FUNCTION_clEnqueueWriteBuffer = 58,
    CL_FUNCTION_clEnqueueWriteBufferRect = 59,
    CL_FUNCTION_clEnqueueWriteImage = 60,
    CL_FUNCTION_clFinish = 61,
    CL_FUNCTION_clFlush = 62,
    CL_FUNCTION_clGetCommandQueueInfo = 63,
    CL_FUNCTION_clGetContextInfo = 64,
    CL_FUNCTION_clGetDeviceAndHostTimer = 65,
    CL_FUNCTION_clGetDeviceIDs = 66,
    CL_FUNCTION_clGetDeviceInfo = 67,
    CL_FUNCTION_clGetEventInfo = 68,
    CL_FUNCTION_clGetEventProfilingInfo = 69,
    CL_FUNCTION_clGetExtensionFunctionAddress = 70,
    CL_FUNCTION_clGetExtensionFunctionAddressForPlatform = 71,
    CL_FUNCTION_clGetGLObjectInfo = 72,
    CL_FUNCTION_clGetGLTextureInfo = 73,
    CL_FUNCTION_clGetHostTimer = 74,
    CL_FUNCTION_clGetImageInfo = 75,
    CL_FUNCTION_clGetKernelArgInfo = 76,
    CL_FUNCTION_clGetKernelInfo = 77,
    CL_FUNCTION_clGetKernelSubGroupInfo = 78,
    CL_FUNCTION_clGetKernelWorkGroupInfo = 79,
    CL_FUNCTION_clGetMemObjectInfo = 80,
    CL_FUNCTION_clGetPipeInfo = 81,
    CL_FUNCTION_clGetPlatformIDs = 82,
    CL_FUNCTION_clGetPlatformInfo = 83,
    CL_FUNCTION_clGetProgramBuildInfo = 84,
    CL_FUNCTION_clGetProgramInfo = 85,
    CL_FUNCTION_clGetSamplerInfo = 86,
    CL_FUNCTION_clGetSupportedImageFormats = 87,
    CL_FUNCTION_clLinkProgram = 88,
    CL_FUNCTION_clReleaseCommandQueue = 89,
    CL_FUNCTION_clReleaseContext = 90,
    CL_FUNCTION_clReleaseDevice = 91,
    CL_FUNCTION_clReleaseEvent = 92,
    CL_FUNCTION_clReleaseKernel = 93,
    CL_FUNCTION_clReleaseMemObject = 94,
    CL_FUNCTION_clReleaseProgram = 95,
    CL_FUNCTION_clReleaseSampler = 96,
    CL_FUNCTION_clRetainCommandQueue = 97,
    CL_FUNCTION_clRetainContext = 98,
    CL_FUNCTION_clRetainDevice = 99,
    CL_FUNCTION_clRetainEvent = 100,
    CL_FUNCTION_clRetainKernel = 101,
    CL_FUNCTION_clRetainMemObject = 102,
    CL_FUNCTION_clRetainProgram = 103,
    CL_FUNCTION_clRetainSampler = 104,
    CL_FUNCTION_clSVMAlloc = 105,
    CL_FUNCTION_clSVMFree = 106,
    CL_FUNCTION_clSetCommandQueueProperty = 107,
    CL_FUNCTION_clSetDefaultDeviceCommandQueue = 108,
    CL_FUNCTION_clSetEventCallback = 109,
    CL_FUNCTION_clSetKernelArg = 110,
    CL_FUNCTION_clSetKernelArgSVMPointer = 111,
    CL_FUNCTION_clSetKernelExecInfo = 112,
    CL_FUNCTION_clSetMemObjectDestructorCallback = 113,
    CL_FUNCTION_clSetUserEventStatus = 114,
    CL_FUNCTION_clUnloadCompiler = 115,
    CL_FUNCTION_clUnloadPlatformCompiler = 116,
    CL_FUNCTION_clWaitForEvents = 117,
    CL_FUNCTION_COUNT = 118,
} cl_function_id;

typedef void (*cl_tracing_callback)(cl_function_id fid, cl_callback_data *callbackData, void *userData);

typedef struct _cl_params_clBuildProgram {
    cl_program *program;
    cl_uint *numDevices;
    const cl_device_id **deviceList;
    const char **options;
    void(CL_CALLBACK **funcNotify)(cl_program program, void *userData);
    void **userData;
} cl_params_clBuildProgram;

typedef struct _cl_params_clCloneKernel {
    cl_kernel *sourceKernel;
    cl_int **errcodeRet;
} cl_params_clCloneKernel;

typedef struct _cl_params_clCompileProgram {
    cl_program *program;
    cl_uint *numDevices;
    const cl_device_id **deviceList;
    const char **options;
    cl_uint *numInputHeaders;
    const cl_program **inputHeaders;
    const char ***headerIncludeNames;
    void(CL_CALLBACK **funcNotify)(cl_program program, void *userData);
    void **userData;
} cl_params_clCompileProgram;

typedef struct _cl_params_clCreateBuffer {
    cl_context *context;
    cl_mem_flags *flags;
    size_t *size;
    void **hostPtr;
    cl_int **errcodeRet;
} cl_params_clCreateBuffer;

typedef struct _cl_params_clCreateCommandQueue {
    cl_context *context;
    cl_device_id *device;
    cl_command_queue_properties *properties;
    cl_int **errcodeRet;
} cl_params_clCreateCommandQueue;

typedef struct _cl_params_clCreateCommandQueueWithProperties {
    cl_context *context;
    cl_device_id *device;
    const cl_queue_properties **properties;
    cl_int **errcodeRet;
} cl_params_clCreateCommandQueueWithProperties;

typedef struct _cl_params_clCreateContext {
    const cl_context_properties **properties;
    cl_uint *numDevices;
    const cl_device_id **devices;
    void(CL_CALLBACK **funcNotify)(const char *, const void *, size_t, void *);
    void **userData;
    cl_int **errcodeRet;
} cl_params_clCreateContext;

typedef struct _cl_params_clCreateContextFromType {
    const cl_context_properties **properties;
    cl_device_type *deviceType;
    void(CL_CALLBACK **funcNotify)(const char *, const void *, size_t, void *);
    void **userData;
    cl_int **errcodeRet;
} cl_params_clCreateContextFromType;

typedef struct _cl_params_clCreateFromGLBuffer {
    cl_context *context;
    cl_mem_flags *flags;
    cl_GLuint *bufobj;
    int **errcodeRet;
} cl_params_clCreateFromGLBuffer;

typedef struct _cl_params_clCreateFromGLRenderbuffer {
    cl_context *context;
    cl_mem_flags *flags;
    cl_GLuint *renderbuffer;
    cl_int **errcodeRet;
} cl_params_clCreateFromGLRenderbuffer;

typedef struct _cl_params_clCreateFromGLTexture {
    cl_context *context;
    cl_mem_flags *flags;
    cl_GLenum *target;
    cl_GLint *miplevel;
    cl_GLuint *texture;
    cl_int **errcodeRet;
} cl_params_clCreateFromGLTexture;

typedef struct _cl_params_clCreateFromGLTexture2D {
    cl_context *context;
    cl_mem_flags *flags;
    cl_GLenum *target;
    cl_GLint *miplevel;
    cl_GLuint *texture;
    cl_int **errcodeRet;
} cl_params_clCreateFromGLTexture2D;

typedef struct _cl_params_clCreateFromGLTexture3D {
    cl_context *context;
    cl_mem_flags *flags;
    cl_GLenum *target;
    cl_GLint *miplevel;
    cl_GLuint *texture;
    cl_int **errcodeRet;
} cl_params_clCreateFromGLTexture3D;

typedef struct _cl_params_clCreateImage {
    cl_context *context;
    cl_mem_flags *flags;
    const cl_image_format **imageFormat;
    const cl_image_desc **imageDesc;
    void **hostPtr;
    cl_int **errcodeRet;
} cl_params_clCreateImage;

typedef struct _cl_params_clCreateImage2D {
    cl_context *context;
    cl_mem_flags *flags;
    const cl_image_format **imageFormat;
    size_t *imageWidth;
    size_t *imageHeight;
    size_t *imageRowPitch;
    void **hostPtr;
    cl_int **errcodeRet;
} cl_params_clCreateImage2D;

typedef struct _cl_params_clCreateImage3D {
    cl_context *context;
    cl_mem_flags *flags;
    const cl_image_format **imageFormat;
    size_t *imageWidth;
    size_t *imageHeight;
    size_t *imageDepth;
    size_t *imageRowPitch;
    size_t *imageSlicePitch;
    void **hostPtr;
    cl_int **errcodeRet;
} cl_params_clCreateImage3D;

typedef struct _cl_params_clCreateKernel {
    cl_program *program;
    const char **kernelName;
    cl_int **errcodeRet;
} cl_params_clCreateKernel;

typedef struct _cl_params_clCreateKernelsInProgram {
    cl_program *program;
    cl_uint *numKernels;
    cl_kernel **kernels;
    cl_uint **numKernelsRet;
} cl_params_clCreateKernelsInProgram;

typedef struct _cl_params_clCreatePipe {
    cl_context *context;
    cl_mem_flags *flags;
    cl_uint *pipePacketSize;
    cl_uint *pipeMaxPackets;
    const cl_pipe_properties **properties;
    cl_int **errcodeRet;
} cl_params_clCreatePipe;

typedef struct _cl_params_clCreateProgramWithBinary {
    cl_context *context;
    cl_uint *numDevices;
    const cl_device_id **deviceList;
    const size_t **lengths;
    const unsigned char ***binaries;
    cl_int **binaryStatus;
    cl_int **errcodeRet;
} cl_params_clCreateProgramWithBinary;

typedef struct _cl_params_clCreateProgramWithBuiltInKernels {
    cl_context *context;
    cl_uint *numDevices;
    const cl_device_id **deviceList;
    const char **kernelNames;
    cl_int **errcodeRet;
} cl_params_clCreateProgramWithBuiltInKernels;

typedef struct _cl_params_clCreateProgramWithIL {
    cl_context *context;
    const void **il;
    size_t *length;
    cl_int **errcodeRet;
} cl_params_clCreateProgramWithIL;

typedef struct _cl_params_clCreateProgramWithSource {
    cl_context *context;
    cl_uint *count;
    const char ***strings;
    const size_t **lengths;
    cl_int **errcodeRet;
} cl_params_clCreateProgramWithSource;

typedef struct _cl_params_clCreateSampler {
    cl_context *context;
    cl_bool *normalizedCoords;
    cl_addressing_mode *addressingMode;
    cl_filter_mode *filterMode;
    cl_int **errcodeRet;
} cl_params_clCreateSampler;

typedef struct _cl_params_clCreateSamplerWithProperties {
    cl_context *context;
    const cl_sampler_properties **samplerProperties;
    cl_int **errcodeRet;
} cl_params_clCreateSamplerWithProperties;

typedef struct _cl_params_clCreateSubBuffer {
    cl_mem *buffer;
    cl_mem_flags *flags;
    cl_buffer_create_type *bufferCreateType;
    const void **bufferCreateInfo;
    cl_int **errcodeRet;
} cl_params_clCreateSubBuffer;

typedef struct _cl_params_clCreateSubDevices {
    cl_device_id *inDevice;
    const cl_device_partition_property **properties;
    cl_uint *numDevices;
    cl_device_id **outDevices;
    cl_uint **numDevicesRet;
} cl_params_clCreateSubDevices;

typedef struct _cl_params_clCreateUserEvent {
    cl_context *context;
    cl_int **errcodeRet;
} cl_params_clCreateUserEvent;

typedef struct _cl_params_clEnqueueAcquireGLObjects {
    cl_command_queue *commandQueue;
    cl_uint *numObjects;
    const cl_mem **memObjects;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueAcquireGLObjects;

typedef struct _cl_params_clEnqueueBarrier {
    cl_command_queue *commandQueue;
} cl_params_clEnqueueBarrier;

typedef struct _cl_params_clEnqueueBarrierWithWaitList {
    cl_command_queue *commandQueue;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueBarrierWithWaitList;

typedef struct _cl_params_clEnqueueCopyBuffer {
    cl_command_queue *commandQueue;
    cl_mem *srcBuffer;
    cl_mem *dstBuffer;
    size_t *srcOffset;
    size_t *dstOffset;
    size_t *cb;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueCopyBuffer;

typedef struct _cl_params_clEnqueueCopyBufferRect {
    cl_command_queue *commandQueue;
    cl_mem *srcBuffer;
    cl_mem *dstBuffer;
    const size_t **srcOrigin;
    const size_t **dstOrigin;
    const size_t **region;
    size_t *srcRowPitch;
    size_t *srcSlicePitch;
    size_t *dstRowPitch;
    size_t *dstSlicePitch;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueCopyBufferRect;

typedef struct _cl_params_clEnqueueCopyBufferToImage {
    cl_command_queue *commandQueue;
    cl_mem *srcBuffer;
    cl_mem *dstImage;
    size_t *srcOffset;
    const size_t **dstOrigin;
    const size_t **region;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueCopyBufferToImage;

typedef struct _cl_params_clEnqueueCopyImage {
    cl_command_queue *commandQueue;
    cl_mem *srcImage;
    cl_mem *dstImage;
    const size_t **srcOrigin;
    const size_t **dstOrigin;
    const size_t **region;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueCopyImage;

typedef struct _cl_params_clEnqueueCopyImageToBuffer {
    cl_command_queue *commandQueue;
    cl_mem *srcImage;
    cl_mem *dstBuffer;
    const size_t **srcOrigin;
    const size_t **region;
    size_t *dstOffset;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueCopyImageToBuffer;

typedef struct _cl_params_clEnqueueFillBuffer {
    cl_command_queue *commandQueue;
    cl_mem *buffer;
    const void **pattern;
    size_t *patternSize;
    size_t *offset;
    size_t *size;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueFillBuffer;

typedef struct _cl_params_clEnqueueFillImage {
    cl_command_queue *commandQueue;
    cl_mem *image;
    const void **fillColor;
    const size_t **origin;
    const size_t **region;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueFillImage;

typedef struct _cl_params_clEnqueueMapBuffer {
    cl_command_queue *commandQueue;
    cl_mem *buffer;
    cl_bool *blockingMap;
    cl_map_flags *mapFlags;
    size_t *offset;
    size_t *cb;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
    cl_int **errcodeRet;
} cl_params_clEnqueueMapBuffer;

typedef struct _cl_params_clEnqueueMapImage {
    cl_command_queue *commandQueue;
    cl_mem *image;
    cl_bool *blockingMap;
    cl_map_flags *mapFlags;
    const size_t **origin;
    const size_t **region;
    size_t **imageRowPitch;
    size_t **imageSlicePitch;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
    cl_int **errcodeRet;
} cl_params_clEnqueueMapImage;

typedef struct _cl_params_clEnqueueMarker {
    cl_command_queue *commandQueue;
    cl_event **event;
} cl_params_clEnqueueMarker;

typedef struct _cl_params_clEnqueueMarkerWithWaitList {
    cl_command_queue *commandQueue;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueMarkerWithWaitList;

typedef struct _cl_params_clEnqueueMigrateMemObjects {
    cl_command_queue *commandQueue;
    cl_uint *numMemObjects;
    const cl_mem **memObjects;
    cl_mem_migration_flags *flags;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueMigrateMemObjects;

typedef struct _cl_params_clEnqueueNDRangeKernel {
    cl_command_queue *commandQueue;
    cl_kernel *kernel;
    cl_uint *workDim;
    const size_t **globalWorkOffset;
    const size_t **globalWorkSize;
    const size_t **localWorkSize;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueNDRangeKernel;

typedef struct _cl_params_clEnqueueNativeKernel {
    cl_command_queue *commandQueue;
    void(CL_CALLBACK **userFunc)(void *);
    void **args;
    size_t *cbArgs;
    cl_uint *numMemObjects;
    const cl_mem **memList;
    const void ***argsMemLoc;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueNativeKernel;

typedef struct _cl_params_clEnqueueReadBuffer {
    cl_command_queue *commandQueue;
    cl_mem *buffer;
    cl_bool *blockingRead;
    size_t *offset;
    size_t *cb;
    void **ptr;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueReadBuffer;

typedef struct _cl_params_clEnqueueReadBufferRect {
    cl_command_queue *commandQueue;
    cl_mem *buffer;
    cl_bool *blockingRead;
    const size_t **bufferOrigin;
    const size_t **hostOrigin;
    const size_t **region;
    size_t *bufferRowPitch;
    size_t *bufferSlicePitch;
    size_t *hostRowPitch;
    size_t *hostSlicePitch;
    void **ptr;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueReadBufferRect;

typedef struct _cl_params_clEnqueueReadImage {
    cl_command_queue *commandQueue;
    cl_mem *image;
    cl_bool *blockingRead;
    const size_t **origin;
    const size_t **region;
    size_t *rowPitch;
    size_t *slicePitch;
    void **ptr;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueReadImage;

typedef struct _cl_params_clEnqueueReleaseGLObjects {
    cl_command_queue *commandQueue;
    cl_uint *numObjects;
    const cl_mem **memObjects;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueReleaseGLObjects;

typedef struct _cl_params_clEnqueueSVMFree {
    cl_command_queue *commandQueue;
    cl_uint *numSvmPointers;
    void ***svmPointers;
    void(CL_CALLBACK **pfnFreeFunc)(cl_command_queue queue, cl_uint numSvmPointers, void **svmPointers, void *userData);
    void **userData;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueSVMFree;

typedef struct _cl_params_clEnqueueSVMMap {
    cl_command_queue *commandQueue;
    cl_bool *blockingMap;
    cl_map_flags *mapFlags;
    void **svmPtr;
    size_t *size;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueSVMMap;

typedef struct _cl_params_clEnqueueSVMMemFill {
    cl_command_queue *commandQueue;
    void **svmPtr;
    const void **pattern;
    size_t *patternSize;
    size_t *size;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueSVMMemFill;

typedef struct _cl_params_clEnqueueSVMMemcpy {
    cl_command_queue *commandQueue;
    cl_bool *blockingCopy;
    void **dstPtr;
    const void **srcPtr;
    size_t *size;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueSVMMemcpy;

typedef struct _cl_params_clEnqueueSVMMigrateMem {
    cl_command_queue *commandQueue;
    cl_uint *numSvmPointers;
    const void ***svmPointers;
    const size_t **sizes;
    const cl_mem_migration_flags *flags;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueSVMMigrateMem;

typedef struct _cl_params_clEnqueueSVMUnmap {
    cl_command_queue *commandQueue;
    void **svmPtr;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueSVMUnmap;

typedef struct _cl_params_clEnqueueTask {
    cl_command_queue *commandQueue;
    cl_kernel *kernel;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueTask;

typedef struct _cl_params_clEnqueueUnmapMemObject {
    cl_command_queue *commandQueue;
    cl_mem *memobj;
    void **mappedPtr;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueUnmapMemObject;

typedef struct _cl_params_clEnqueueWaitForEvents {
    cl_command_queue *commandQueue;
    cl_uint *numEvents;
    const cl_event **eventList;
} cl_params_clEnqueueWaitForEvents;

typedef struct _cl_params_clEnqueueWriteBuffer {
    cl_command_queue *commandQueue;
    cl_mem *buffer;
    cl_bool *blockingWrite;
    size_t *offset;
    size_t *cb;
    const void **ptr;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueWriteBuffer;

typedef struct _cl_params_clEnqueueWriteBufferRect {
    cl_command_queue *commandQueue;
    cl_mem *buffer;
    cl_bool *blockingWrite;
    const size_t **bufferOrigin;
    const size_t **hostOrigin;
    const size_t **region;
    size_t *bufferRowPitch;
    size_t *bufferSlicePitch;
    size_t *hostRowPitch;
    size_t *hostSlicePitch;
    const void **ptr;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueWriteBufferRect;

typedef struct _cl_params_clEnqueueWriteImage {
    cl_command_queue *commandQueue;
    cl_mem *image;
    cl_bool *blockingWrite;
    const size_t **origin;
    const size_t **region;
    size_t *inputRowPitch;
    size_t *inputSlicePitch;
    const void **ptr;
    cl_uint *numEventsInWaitList;
    const cl_event **eventWaitList;
    cl_event **event;
} cl_params_clEnqueueWriteImage;

typedef struct _cl_params_clFinish {
    cl_command_queue *commandQueue;
} cl_params_clFinish;

typedef struct _cl_params_clFlush {
    cl_command_queue *commandQueue;
} cl_params_clFlush;

typedef struct _cl_params_clGetCommandQueueInfo {
    cl_command_queue *commandQueue;
    cl_command_queue_info *paramName;
    size_t *paramValueSize;
    void **paramValue;
    size_t **paramValueSizeRet;
} cl_params_clGetCommandQueueInfo;

typedef struct _cl_params_clGetContextInfo {
    cl_context *context;
    cl_context_info *paramName;
    size_t *paramValueSize;
    void **paramValue;
    size_t **paramValueSizeRet;
} cl_params_clGetContextInfo;

typedef struct _cl_params_clGetDeviceAndHostTimer {
    cl_device_id *device;
    cl_ulong **deviceTimestamp;
    cl_ulong **hostTimestamp;
} cl_params_clGetDeviceAndHostTimer;

typedef struct _cl_params_clGetDeviceIDs {
    cl_platform_id *platform;
    cl_device_type *deviceType;
    cl_uint *numEntries;
    cl_device_id **devices;
    cl_uint **numDevices;
} cl_params_clGetDeviceIDs;

typedef struct _cl_params_clGetDeviceInfo {
    cl_device_id *device;
    cl_device_info *paramName;
    size_t *paramValueSize;
    void **paramValue;
    size_t **paramValueSizeRet;
} cl_params_clGetDeviceInfo;

typedef struct _cl_params_clGetEventInfo {
    cl_event *event;
    cl_event_info *paramName;
    size_t *paramValueSize;
    void **paramValue;
    size_t **paramValueSizeRet;
} cl_params_clGetEventInfo;

typedef struct _cl_params_clGetEventProfilingInfo {
    cl_event *event;
    cl_profiling_info *paramName;
    size_t *paramValueSize;
    void **paramValue;
    size_t **paramValueSizeRet;
} cl_params_clGetEventProfilingInfo;

typedef struct _cl_params_clGetExtensionFunctionAddress {
    const char **funcName;
} cl_params_clGetExtensionFunctionAddress;

typedef struct _cl_params_clGetExtensionFunctionAddressForPlatform {
    cl_platform_id *platform;
    const char **funcName;
} cl_params_clGetExtensionFunctionAddressForPlatform;

typedef struct _cl_params_clGetGLObjectInfo {
    cl_mem *memobj;
    cl_gl_object_type **glObjectType;
    cl_GLuint **glObjectName;
} cl_params_clGetGLObjectInfo;

typedef struct _cl_params_clGetGLTextureInfo {
    cl_mem *memobj;
    cl_gl_texture_info *paramName;
    size_t *paramValueSize;
    void **paramValue;
    size_t **paramValueSizeRet;
} cl_params_clGetGLTextureInfo;

typedef struct _cl_params_clGetHostTimer {
    cl_device_id *device;
    cl_ulong **hostTimestamp;
} cl_params_clGetHostTimer;

typedef struct _cl_params_clGetImageInfo {
    cl_mem *image;
    cl_image_info *paramName;
    size_t *paramValueSize;
    void **paramValue;
    size_t **paramValueSizeRet;
} cl_params_clGetImageInfo;

typedef struct _cl_params_clGetKernelArgInfo {
    cl_kernel *kernel;
    cl_uint *argIndx;
    cl_kernel_arg_info *paramName;
    size_t *paramValueSize;
    void **paramValue;
    size_t **paramValueSizeRet;
} cl_params_clGetKernelArgInfo;

typedef struct _cl_params_clGetKernelInfo {
    cl_kernel *kernel;
    cl_kernel_info *paramName;
    size_t *paramValueSize;
    void **paramValue;
    size_t **paramValueSizeRet;
} cl_params_clGetKernelInfo;

typedef struct _cl_params_clGetKernelSubGroupInfo {
    cl_kernel *kernel;
    cl_device_id *device;
    cl_kernel_sub_group_info *paramName;
    size_t *inputValueSize;
    const void **inputValue;
    size_t *paramValueSize;
    void **paramValue;
    size_t **paramValueSizeRet;
} cl_params_clGetKernelSubGroupInfo;

typedef struct _cl_params_clGetKernelWorkGroupInfo {
    cl_kernel *kernel;
    cl_device_id *device;
    cl_kernel_work_group_info *paramName;
    size_t *paramValueSize;
    void **paramValue;
    size_t **paramValueSizeRet;
} cl_params_clGetKernelWorkGroupInfo;

typedef struct _cl_params_clGetMemObjectInfo {
    cl_mem *memobj;
    cl_mem_info *paramName;
    size_t *paramValueSize;
    void **paramValue;
    size_t **paramValueSizeRet;
} cl_params_clGetMemObjectInfo;

typedef struct _cl_params_clGetPipeInfo {
    cl_mem *pipe;
    cl_pipe_info *paramName;
    size_t *paramValueSize;
    void **paramValue;
    size_t **paramValueSizeRet;
} cl_params_clGetPipeInfo;

typedef struct _cl_params_clGetPlatformIDs {
    cl_uint *numEntries;
    cl_platform_id **platforms;
    cl_uint **numPlatforms;
} cl_params_clGetPlatformIDs;

typedef struct _cl_params_clGetPlatformInfo {
    cl_platform_id *platform;
    cl_platform_info *paramName;
    size_t *paramValueSize;
    void **paramValue;
    size_t **paramValueSizeRet;
} cl_params_clGetPlatformInfo;

typedef struct _cl_params_clGetProgramBuildInfo {
    cl_program *program;
    cl_device_id *device;
    cl_program_build_info *paramName;
    size_t *paramValueSize;
    void **paramValue;
    size_t **paramValueSizeRet;
} cl_params_clGetProgramBuildInfo;

typedef struct _cl_params_clGetProgramInfo {
    cl_program *program;
    cl_program_info *paramName;
    size_t *paramValueSize;
    void **paramValue;
    size_t **paramValueSizeRet;
} cl_params_clGetProgramInfo;

typedef struct _cl_params_clGetSamplerInfo {
    cl_sampler *sampler;
    cl_sampler_info *paramName;
    size_t *paramValueSize;
    void **paramValue;
    size_t **paramValueSizeRet;
} cl_params_clGetSamplerInfo;

typedef struct _cl_params_clGetSupportedImageFormats {
    cl_context *context;
    cl_mem_flags *flags;
    cl_mem_object_type *imageType;
    cl_uint *numEntries;
    cl_image_format **imageFormats;
    cl_uint **numImageFormats;
} cl_params_clGetSupportedImageFormats;

typedef struct _cl_params_clLinkProgram {
    cl_context *context;
    cl_uint *numDevices;
    const cl_device_id **deviceList;
    const char **options;
    cl_uint *numInputPrograms;
    const cl_program **inputPrograms;
    void(CL_CALLBACK **funcNotify)(cl_program program, void *userData);
    void **userData;
    cl_int **errcodeRet;
} cl_params_clLinkProgram;

typedef struct _cl_params_clReleaseCommandQueue {
    cl_command_queue *commandQueue;
} cl_params_clReleaseCommandQueue;

typedef struct _cl_params_clReleaseContext {
    cl_context *context;
} cl_params_clReleaseContext;

typedef struct _cl_params_clReleaseDevice {
    cl_device_id *device;
} cl_params_clReleaseDevice;

typedef struct _cl_params_clReleaseEvent {
    cl_event *event;
} cl_params_clReleaseEvent;

typedef struct _cl_params_clReleaseKernel {
    cl_kernel *kernel;
} cl_params_clReleaseKernel;

typedef struct _cl_params_clReleaseMemObject {
    cl_mem *memobj;
} cl_params_clReleaseMemObject;

typedef struct _cl_params_clReleaseProgram {
    cl_program *program;
} cl_params_clReleaseProgram;

typedef struct _cl_params_clReleaseSampler {
    cl_sampler *sampler;
} cl_params_clReleaseSampler;

typedef struct _cl_params_clRetainCommandQueue {
    cl_command_queue *commandQueue;
} cl_params_clRetainCommandQueue;

typedef struct _cl_params_clRetainContext {
    cl_context *context;
} cl_params_clRetainContext;

typedef struct _cl_params_clRetainDevice {
    cl_device_id *device;
} cl_params_clRetainDevice;

typedef struct _cl_params_clRetainEvent {
    cl_event *event;
} cl_params_clRetainEvent;

typedef struct _cl_params_clRetainKernel {
    cl_kernel *kernel;
} cl_params_clRetainKernel;

typedef struct _cl_params_clRetainMemObject {
    cl_mem *memobj;
} cl_params_clRetainMemObject;

typedef struct _cl_params_clRetainProgram {
    cl_program *program;
} cl_params_clRetainProgram;

typedef struct _cl_params_clRetainSampler {
    cl_sampler *sampler;
} cl_params_clRetainSampler;

typedef struct _cl_params_clSVMAlloc {
    cl_context *context;
    cl_svm_mem_flags *flags;
    size_t *size;
    cl_uint *alignment;
} cl_params_clSVMAlloc;

typedef struct _cl_params_clSVMFree {
    cl_context *context;
    void **svmPointer;
} cl_params_clSVMFree;

typedef struct _cl_params_clSetCommandQueueProperty {
    cl_command_queue *commandQueue;
    cl_command_queue_properties *properties;
    cl_bool *enable;
    cl_command_queue_properties **oldProperties;
} cl_params_clSetCommandQueueProperty;

typedef struct _cl_params_clSetDefaultDeviceCommandQueue {
    cl_context *context;
    cl_device_id *device;
    cl_command_queue *commandQueue;
} cl_params_clSetDefaultDeviceCommandQueue;

typedef struct _cl_params_clSetEventCallback {
    cl_event *event;
    cl_int *commandExecCallbackType;
    void(CL_CALLBACK **funcNotify)(cl_event, cl_int, void *);
    void **userData;
} cl_params_clSetEventCallback;

typedef struct _cl_params_clSetKernelArg {
    cl_kernel *kernel;
    cl_uint *argIndex;
    size_t *argSize;
    const void **argValue;
} cl_params_clSetKernelArg;

typedef struct _cl_params_clSetKernelArgSVMPointer {
    cl_kernel *kernel;
    cl_uint *argIndex;
    const void **argValue;
} cl_params_clSetKernelArgSVMPointer;

typedef struct _cl_params_clSetKernelExecInfo {
    cl_kernel *kernel;
    cl_kernel_exec_info *paramName;
    size_t *paramValueSize;
    const void **paramValue;
} cl_params_clSetKernelExecInfo;

typedef struct _cl_params_clSetMemObjectDestructorCallback {
    cl_mem *memobj;
    void(CL_CALLBACK **funcNotify)(cl_mem, void *);
    void **userData;
} cl_params_clSetMemObjectDestructorCallback;

typedef struct _cl_params_clSetUserEventStatus {
    cl_event *event;
    cl_int *executionStatus;
} cl_params_clSetUserEventStatus;

typedef struct _cl_params_clUnloadCompiler {
} cl_params_clUnloadCompiler;

typedef struct _cl_params_clUnloadPlatformCompiler {
    cl_platform_id *platform;
} cl_params_clUnloadPlatformCompiler;

typedef struct _cl_params_clWaitForEvents {
    cl_uint *numEvents;
    const cl_event **eventList;
} cl_params_clWaitForEvents;
