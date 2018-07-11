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
#include "CL/cl_ext.h"
#include <cstdint>

// clang-format off
struct DeviceInfo {
    cl_device_type               deviceType;
    cl_uint                      vendorId;
    cl_uint                      maxComputUnits;
    cl_uint                      maxWorkItemDimensions;
    size_t                       maxWorkItemSizes[3];
    size_t                       maxWorkGroupSize;
    cl_uint                      maxNumOfSubGroups;
    size_t                       maxNumEUsPerSubSlice;
    size_t                       maxSubGroups[3];
    cl_bool                      independentForwardProgress;
    cl_uint                      preferredVectorWidthChar;
    cl_uint                      preferredVectorWidthShort;
    cl_uint                      preferredVectorWidthInt;
    cl_uint                      preferredVectorWidthLong;
    cl_uint                      preferredVectorWidthFloat;
    cl_uint                      preferredVectorWidthDouble;
    cl_uint                      preferredVectorWidthHalf;
    cl_uint                      nativeVectorWidthChar;
    cl_uint                      nativeVectorWidthShort;
    cl_uint                      nativeVectorWidthInt;
    cl_uint                      nativeVectorWidthLong;
    cl_uint                      nativeVectorWidthFloat;
    cl_uint                      nativeVectorWidthDouble;
    cl_uint                      nativeVectorWidthHalf;
    cl_uint                      numThreadsPerEU;
    cl_uint                      maxClockFrequency;
    cl_uint                      addressBits;
    cl_ulong                     maxMemAllocSize;
    cl_bool                      imageSupport;
    cl_uint                      maxReadImageArgs;
    cl_uint                      maxWriteImageArgs;
    cl_uint                      maxReadWriteImageArgs;
    size_t                       imageMaxBufferSize;
    size_t                       image2DMaxWidth;
    size_t                       image2DMaxHeight;
    size_t                       image3DMaxWidth;
    size_t                       image3DMaxHeight;
    size_t                       image3DMaxDepth;
    size_t                       imageMaxArraySize;
    size_t                       maxBufferSize;
    size_t                       maxArraySize;
    cl_uint                      maxSamplers;
    cl_uint                      imagePitchAlignment;
    cl_uint                      imageBaseAddressAlignment;
    cl_uint                      maxPipeArgs;
    cl_uint                      pipeMaxActiveReservations;
    cl_uint                      pipeMaxPacketSize;
    size_t                       maxParameterSize;
    cl_uint                      memBaseAddressAlign;
    cl_uint                      minDataTypeAlignSize;
    cl_device_fp_config          singleFpConfig;
    cl_device_fp_config          halfFpConfig;
    cl_device_fp_config          doubleFpConfig;
    cl_device_mem_cache_type     globalMemCacheType;
    cl_uint                      globalMemCachelineSize;
    cl_ulong                     globalMemCacheSize;
    cl_ulong                     globalMemSize;
    cl_ulong                     maxConstantBufferSize;
    cl_uint                      maxConstantArgs;
    size_t                       maxGlobalVariableSize;
    size_t                       globalVariablePreferredTotalSize;
    cl_device_local_mem_type     localMemType;
    cl_ulong                     localMemSize;
    cl_bool                      errorCorrectionSupport;
    double                       profilingTimerResolution;
    size_t                       outProfilingTimerResolution;
    cl_bool                      endianLittle;
    cl_bool                      deviceAvailable;
    cl_bool                      compilerAvailable;
    cl_bool                      linkerAvailable;
    cl_device_exec_capabilities  executionCapabilities;
    cl_command_queue_properties  queueOnHostProperties;
    cl_command_queue_properties  queueOnDeviceProperties;
    cl_uint                      queueOnDevicePreferredSize;
    cl_uint                      queueOnDeviceMaxSize;
    cl_uint                      maxOnDeviceQueues;
    cl_uint                      maxOnDeviceEvents;
    const char                   *builtInKernels;
    cl_platform_id               platform;
    const char                   *name;
    const char                   *vendor;
    const char                   *driverVersion;
    const char                   *profile;
    const char                   *clVersion;
    const char                   *clCVersion;
    const char                   *spirVersions;
    const char                   *deviceExtensions;
    size_t                       printfBufferSize;
    cl_bool                      preferredInteropUserSync;
    cl_device_id                 parentDevice;
    cl_uint                      partitionMaxSubDevices;
    cl_device_partition_property partitionProperties;
    cl_device_affinity_domain    partitionAffinityDomain;
    cl_device_partition_property *partitionType;
    cl_uint                      referenceCount;
    cl_device_svm_capabilities   svmCapabilities;
    cl_uint                      preferredPlatformAtomicAlignment;
    cl_uint                      preferredGlobalAtomicAlignment;
    cl_uint                      preferredLocalAtomicAlignment;
    cl_bool                      hostUnifiedMemory;
    const char                  *ilVersion;
    uint32_t                     computeUnitsUsedForScratch;
    bool                         force32BitAddressess;
    bool                         preemptionSupported;
    double                       platformHostTimerResolution;

    size_t                       planarYuvMaxWidth;
    size_t                       planarYuvMaxHeight;

    cl_bool                      vmeAvcSupportsPreemption;
    cl_bool                      vmeAvcSupportsTextureSampler;
    cl_uint                      vmeAvcVersion;

    cl_uint                      vmeVersion;

    /* Extensions supported */
    bool                         nv12Extension;
    bool                         vmeExtension;
    bool                         platformLP;
    bool                         cpuCopyAllowed;
    bool                         packedYuvExtension;
    cl_uint                      internalDriverVersion;
    bool                         sourceLevelDebuggerActive;
};
// clang-format on
