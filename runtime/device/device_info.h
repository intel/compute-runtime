/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "public/cl_ext_private.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

#include <cstdint>

// clang-format off
struct DeviceInfo {
    cl_device_type               deviceType;
    size_t                       maxWorkGroupSize;
    size_t                       maxNumEUsPerSubSlice;
    size_t                       maxSliceCount;
    cl_ulong                     maxMemAllocSize;
    size_t                       imageMaxBufferSize;
    size_t                       image2DMaxWidth;
    size_t                       image2DMaxHeight;
    size_t                       image3DMaxWidth;
    size_t                       image3DMaxHeight;
    size_t                       image3DMaxDepth;
    size_t                       imageMaxArraySize;
    size_t                       maxBufferSize;
    size_t                       maxArraySize;
    size_t                       maxParameterSize;
    cl_device_fp_config          singleFpConfig;
    cl_device_fp_config          halfFpConfig;
    cl_device_fp_config          doubleFpConfig;
    cl_ulong                     globalMemCacheSize;
    cl_ulong                     globalMemSize;
    cl_ulong                     maxConstantBufferSize;
    size_t                       maxGlobalVariableSize;
    size_t                       globalVariablePreferredTotalSize;
    cl_ulong                     localMemSize;
    double                       profilingTimerResolution;
    size_t                       outProfilingTimerResolution;
    cl_device_exec_capabilities  executionCapabilities;
    cl_command_queue_properties  queueOnHostProperties;
    cl_command_queue_properties  queueOnDeviceProperties;
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
    cl_device_id                 parentDevice;
    cl_device_partition_property partitionProperties;
    cl_device_affinity_domain    partitionAffinityDomain;
    cl_device_partition_property *partitionType;
    cl_device_svm_capabilities   svmCapabilities;
    const char                  *ilVersion;
    double                       platformHostTimerResolution;
    size_t                       planarYuvMaxWidth;
    size_t                       planarYuvMaxHeight;
    size_t                       maxWorkItemSizes[3];
    size_t                       maxSubGroups[3];
    cl_uint                      vendorId;
    cl_uint                      maxComputUnits;
    cl_uint                      maxWorkItemDimensions;
    cl_uint                      maxNumOfSubGroups;
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
    cl_bool                      imageSupport;
    cl_uint                      maxReadImageArgs;
    cl_uint                      maxWriteImageArgs;
    cl_uint                      maxReadWriteImageArgs;
    cl_uint                      maxSamplers;
    cl_uint                      imagePitchAlignment;
    cl_uint                      imageBaseAddressAlignment;
    cl_uint                      maxPipeArgs;
    cl_uint                      pipeMaxActiveReservations;
    cl_uint                      pipeMaxPacketSize;
    cl_uint                      memBaseAddressAlign;
    cl_uint                      minDataTypeAlignSize;
    cl_device_mem_cache_type     globalMemCacheType;
    cl_uint                      globalMemCachelineSize;
    cl_uint                      maxConstantArgs;
    cl_device_local_mem_type     localMemType;
    cl_bool                      errorCorrectionSupport;
    cl_bool                      endianLittle;
    cl_bool                      deviceAvailable;
    cl_bool                      compilerAvailable;
    cl_bool                      linkerAvailable;
    cl_uint                      queueOnDevicePreferredSize;
    cl_uint                      queueOnDeviceMaxSize;
    cl_uint                      maxOnDeviceQueues;
    cl_uint                      maxOnDeviceEvents;
    cl_bool                      preferredInteropUserSync;
    cl_uint                      partitionMaxSubDevices;
    cl_uint                      referenceCount;
    cl_uint                      preferredPlatformAtomicAlignment;
    cl_uint                      preferredGlobalAtomicAlignment;
    cl_uint                      preferredLocalAtomicAlignment;
    cl_bool                      hostUnifiedMemory;
    uint32_t                     computeUnitsUsedForScratch;
    uint32_t                     maxFrontEndThreads;
    cl_bool                      vmeAvcSupportsPreemption;
    cl_bool                      vmeAvcSupportsTextureSampler;
    cl_uint                      vmeAvcVersion;
    cl_uint                      vmeVersion;
    cl_uint                      internalDriverVersion;
    cl_uint                      grfSize;
    bool                         force32BitAddressess;
    bool                         preemptionSupported;
    /* Extensions supported */
    bool                         nv12Extension;
    bool                         vmeExtension;
    bool                         platformLP;
    bool                         packedYuvExtension;
    bool                         debuggerActive;
    /*Unified Shared Memory Capabilites*/
    cl_unified_shared_memory_capabilities_intel hostMemCapabilities;
    cl_unified_shared_memory_capabilities_intel deviceMemCapabilities;
    cl_unified_shared_memory_capabilities_intel singleDeviceSharedMemCapabilities;
    cl_unified_shared_memory_capabilities_intel crossDeviceSharedMemCapabilities;
    cl_unified_shared_memory_capabilities_intel sharedSystemMemCapabilities;
};
// clang-format on
