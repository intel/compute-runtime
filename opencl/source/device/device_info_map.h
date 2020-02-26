/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/source/device/device_info.h"

#include "CL/cl_ext_intel.h"
#include <CL/cl.h>
#include <CL/cl_ext.h>

namespace DeviceInfoTable {
template <cl_device_info Param, typename _Type, _Type DeviceInfo::*val>
struct MapBase {
    enum { param = Param };
    typedef _Type Type;
    enum { size = sizeof(Type) };

    static const Type &getValue(const DeviceInfo &deviceInfo) {
        return deviceInfo.*val;
    }
};

template <cl_device_info Param>
struct Map {};

//////////////////////////////////////////////////////
// DeviceInfo mapping table
// Map<Param>::param    - i.e. CL_DEVICE_ADDRESS_BITS
// Map<Param>::Type     - i.e. cl_uint
// Map<Param>::size     - ie. sizeof( cl_uint )
// Map<Param>::getValue - ie. return deviceInfo.AddressBits
//////////////////////////////////////////////////////
// clang-format off
// please keep alphabetical order
template<> struct Map<CL_DEVICE_ADDRESS_BITS                                > : public MapBase<CL_DEVICE_ADDRESS_BITS,                                unsigned int,                    &DeviceInfo::addressBits> {};
template<> struct Map<CL_DEVICE_AVAILABLE                                   > : public MapBase<CL_DEVICE_AVAILABLE,                                   uint32_t,                        &DeviceInfo::deviceAvailable> {};
template<> struct Map<CL_DEVICE_AVC_ME_SUPPORTS_PREEMPTION_INTEL            > : public MapBase<CL_DEVICE_AVC_ME_SUPPORTS_PREEMPTION_INTEL,            uint32_t,                        &DeviceInfo::vmeAvcSupportsPreemption> {};
template<> struct Map<CL_DEVICE_AVC_ME_SUPPORTS_TEXTURE_SAMPLER_USE_INTEL   > : public MapBase<CL_DEVICE_AVC_ME_SUPPORTS_TEXTURE_SAMPLER_USE_INTEL,   uint32_t,                        &DeviceInfo::vmeAvcSupportsTextureSampler> {};
template<> struct Map<CL_DEVICE_AVC_ME_VERSION_INTEL                        > : public MapBase<CL_DEVICE_AVC_ME_VERSION_INTEL,                        uint32_t,                        &DeviceInfo::vmeAvcVersion> {};
template<> struct Map<CL_DEVICE_BUILT_IN_KERNELS                            > : public MapBase<CL_DEVICE_BUILT_IN_KERNELS,                            const char *,                    &DeviceInfo::builtInKernels> {};
template<> struct Map<CL_DEVICE_COMPILER_AVAILABLE                          > : public MapBase<CL_DEVICE_COMPILER_AVAILABLE,                          uint32_t,                        &DeviceInfo::compilerAvailable> {};
template<> struct Map<CL_DEVICE_CROSS_DEVICE_SHARED_MEM_CAPABILITIES_INTEL  > : public MapBase<CL_DEVICE_CROSS_DEVICE_SHARED_MEM_CAPABILITIES_INTEL,  uint64_t,                        &DeviceInfo::crossDeviceSharedMemCapabilities> {};
template<> struct Map<CL_DEVICE_DEVICE_MEM_CAPABILITIES_INTEL               > : public MapBase<CL_DEVICE_DEVICE_MEM_CAPABILITIES_INTEL,               uint64_t,                        &DeviceInfo::deviceMemCapabilities> {};
template<> struct Map<CL_DEVICE_DOUBLE_FP_CONFIG                            > : public MapBase<CL_DEVICE_DOUBLE_FP_CONFIG,                            uint64_t,                        &DeviceInfo::doubleFpConfig> {};
template<> struct Map<CL_DEVICE_DRIVER_VERSION_INTEL                        > : public MapBase<CL_DEVICE_DRIVER_VERSION_INTEL,                        uint32_t,                        &DeviceInfo::internalDriverVersion> {};
template<> struct Map<CL_DEVICE_ENDIAN_LITTLE                               > : public MapBase<CL_DEVICE_ENDIAN_LITTLE,                               uint32_t,                        &DeviceInfo::endianLittle> {};
template<> struct Map<CL_DEVICE_ERROR_CORRECTION_SUPPORT                    > : public MapBase<CL_DEVICE_ERROR_CORRECTION_SUPPORT,                    uint32_t,                        &DeviceInfo::errorCorrectionSupport> {};
template<> struct Map<CL_DEVICE_EXECUTION_CAPABILITIES                      > : public MapBase<CL_DEVICE_EXECUTION_CAPABILITIES,                      uint64_t,                        &DeviceInfo::executionCapabilities> {};
template<> struct Map<CL_DEVICE_EXTENSIONS                                  > : public MapBase<CL_DEVICE_EXTENSIONS,                                  const char *,                    &DeviceInfo::deviceExtensions> {};
template<> struct Map<CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE                   > : public MapBase<CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE,                   unsigned int,                    &DeviceInfo::globalMemCachelineSize> {};
template<> struct Map<CL_DEVICE_GLOBAL_MEM_CACHE_SIZE                       > : public MapBase<CL_DEVICE_GLOBAL_MEM_CACHE_SIZE,                       uint64_t,                        &DeviceInfo::globalMemCacheSize> {};
template<> struct Map<CL_DEVICE_GLOBAL_MEM_CACHE_TYPE                       > : public MapBase<CL_DEVICE_GLOBAL_MEM_CACHE_TYPE,                       uint32_t,                        &DeviceInfo::globalMemCacheType> {};
template<> struct Map<CL_DEVICE_GLOBAL_MEM_SIZE                             > : public MapBase<CL_DEVICE_GLOBAL_MEM_SIZE,                             uint64_t,                        &DeviceInfo::globalMemSize> {};
template<> struct Map<CL_DEVICE_GLOBAL_VARIABLE_PREFERRED_TOTAL_SIZE        > : public MapBase<CL_DEVICE_GLOBAL_VARIABLE_PREFERRED_TOTAL_SIZE,        size_t,                          &DeviceInfo::globalVariablePreferredTotalSize> {};
template<> struct Map<CL_DEVICE_HALF_FP_CONFIG                              > : public MapBase<CL_DEVICE_HALF_FP_CONFIG,                              uint64_t,                        &DeviceInfo::halfFpConfig> {};
template<> struct Map<CL_DEVICE_HOST_MEM_CAPABILITIES_INTEL                 > : public MapBase<CL_DEVICE_HOST_MEM_CAPABILITIES_INTEL,                 uint64_t,                        &DeviceInfo::hostMemCapabilities> {};
template<> struct Map<CL_DEVICE_HOST_UNIFIED_MEMORY                         > : public MapBase<CL_DEVICE_HOST_UNIFIED_MEMORY,                         uint32_t,                        &DeviceInfo::hostUnifiedMemory> {};
template<> struct Map<CL_DEVICE_IL_VERSION                                  > : public MapBase<CL_DEVICE_IL_VERSION,                                  const char *,                    &DeviceInfo::ilVersion> {};
template<> struct Map<CL_DEVICE_IMAGE2D_MAX_HEIGHT                          > : public MapBase<CL_DEVICE_IMAGE2D_MAX_HEIGHT,                          size_t,                          &DeviceInfo::image2DMaxHeight> {};
template<> struct Map<CL_DEVICE_IMAGE2D_MAX_WIDTH                           > : public MapBase<CL_DEVICE_IMAGE2D_MAX_WIDTH,                           size_t,                          &DeviceInfo::image2DMaxWidth> {};
template<> struct Map<CL_DEVICE_IMAGE3D_MAX_DEPTH                           > : public MapBase<CL_DEVICE_IMAGE3D_MAX_DEPTH,                           size_t,                          &DeviceInfo::image3DMaxDepth> {};
template<> struct Map<CL_DEVICE_IMAGE3D_MAX_HEIGHT                          > : public MapBase<CL_DEVICE_IMAGE3D_MAX_HEIGHT,                          size_t,                          &DeviceInfo::image3DMaxHeight> {};
template<> struct Map<CL_DEVICE_IMAGE3D_MAX_WIDTH                           > : public MapBase<CL_DEVICE_IMAGE3D_MAX_WIDTH,                           size_t,                          &DeviceInfo::image3DMaxWidth> {};
template<> struct Map<CL_DEVICE_IMAGE_BASE_ADDRESS_ALIGNMENT                > : public MapBase<CL_DEVICE_IMAGE_BASE_ADDRESS_ALIGNMENT,                uint32_t,                        &DeviceInfo::imageBaseAddressAlignment> {};
template<> struct Map<CL_DEVICE_IMAGE_MAX_ARRAY_SIZE                        > : public MapBase<CL_DEVICE_IMAGE_MAX_ARRAY_SIZE,                        size_t,                          &DeviceInfo::imageMaxArraySize> {};
template<> struct Map<CL_DEVICE_IMAGE_MAX_BUFFER_SIZE                       > : public MapBase<CL_DEVICE_IMAGE_MAX_BUFFER_SIZE,                       size_t,                          &DeviceInfo::imageMaxBufferSize> {};
template<> struct Map<CL_DEVICE_IMAGE_PITCH_ALIGNMENT                       > : public MapBase<CL_DEVICE_IMAGE_PITCH_ALIGNMENT,                       uint32_t,                        &DeviceInfo::imagePitchAlignment> {};
template<> struct Map<CL_DEVICE_IMAGE_SUPPORT                               > : public MapBase<CL_DEVICE_IMAGE_SUPPORT,                               uint32_t,                        &DeviceInfo::imageSupport> {};
template<> struct Map<CL_DEVICE_LINKER_AVAILABLE                            > : public MapBase<CL_DEVICE_LINKER_AVAILABLE,                            uint32_t,                        &DeviceInfo::linkerAvailable> {};
template<> struct Map<CL_DEVICE_LOCAL_MEM_SIZE                              > : public MapBase<CL_DEVICE_LOCAL_MEM_SIZE,                              uint64_t,                        &DeviceInfo::localMemSize> {};
template<> struct Map<CL_DEVICE_LOCAL_MEM_TYPE                              > : public MapBase<CL_DEVICE_LOCAL_MEM_TYPE,                              uint32_t,                        &DeviceInfo::localMemType> {};
template<> struct Map<CL_DEVICE_MAX_CLOCK_FREQUENCY                         > : public MapBase<CL_DEVICE_MAX_CLOCK_FREQUENCY,                         uint32_t,                        &DeviceInfo::maxClockFrequency> {};
template<> struct Map<CL_DEVICE_MAX_COMPUTE_UNITS                           > : public MapBase<CL_DEVICE_MAX_COMPUTE_UNITS,                           uint32_t,                        &DeviceInfo::maxComputUnits> {};
template<> struct Map<CL_DEVICE_MAX_CONSTANT_ARGS                           > : public MapBase<CL_DEVICE_MAX_CONSTANT_ARGS,                           uint32_t,                        &DeviceInfo::maxConstantArgs> {};
template<> struct Map<CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE                    > : public MapBase<CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE,                    uint64_t,                        &DeviceInfo::maxConstantBufferSize> {};
template<> struct Map<CL_DEVICE_MAX_GLOBAL_VARIABLE_SIZE                    > : public MapBase<CL_DEVICE_MAX_GLOBAL_VARIABLE_SIZE,                    size_t,                          &DeviceInfo::maxGlobalVariableSize> {};
template<> struct Map<CL_DEVICE_MAX_MEM_ALLOC_SIZE                          > : public MapBase<CL_DEVICE_MAX_MEM_ALLOC_SIZE,                          uint64_t,                        &DeviceInfo::maxMemAllocSize> {};
template<> struct Map<CL_DEVICE_MAX_NUM_SUB_GROUPS                          > : public MapBase<CL_DEVICE_MAX_NUM_SUB_GROUPS,                          uint32_t,                        &DeviceInfo::maxNumOfSubGroups> {};
template<> struct Map<CL_DEVICE_MAX_ON_DEVICE_EVENTS                        > : public MapBase<CL_DEVICE_MAX_ON_DEVICE_EVENTS,                        uint32_t,                        &DeviceInfo::maxOnDeviceEvents> {};
template<> struct Map<CL_DEVICE_MAX_ON_DEVICE_QUEUES                        > : public MapBase<CL_DEVICE_MAX_ON_DEVICE_QUEUES,                        uint32_t,                        &DeviceInfo::maxOnDeviceQueues> {};
template<> struct Map<CL_DEVICE_MAX_PARAMETER_SIZE                          > : public MapBase<CL_DEVICE_MAX_PARAMETER_SIZE,                          size_t,                          &DeviceInfo::maxParameterSize> {};
template<> struct Map<CL_DEVICE_MAX_PIPE_ARGS                               > : public MapBase<CL_DEVICE_MAX_PIPE_ARGS,                               uint32_t,                        &DeviceInfo::maxPipeArgs> {};
template<> struct Map<CL_DEVICE_MAX_READ_IMAGE_ARGS                         > : public MapBase<CL_DEVICE_MAX_READ_IMAGE_ARGS,                         uint32_t,                        &DeviceInfo::maxReadImageArgs> {};
template<> struct Map<CL_DEVICE_MAX_READ_WRITE_IMAGE_ARGS                   > : public MapBase<CL_DEVICE_MAX_READ_WRITE_IMAGE_ARGS,                   uint32_t,                        &DeviceInfo::maxReadWriteImageArgs> {};
template<> struct Map<CL_DEVICE_MAX_SAMPLERS                                > : public MapBase<CL_DEVICE_MAX_SAMPLERS,                                uint32_t,                        &DeviceInfo::maxSamplers> {};
template<> struct Map<CL_DEVICE_MAX_WORK_GROUP_SIZE                         > : public MapBase<CL_DEVICE_MAX_WORK_GROUP_SIZE,                         size_t,                          &DeviceInfo::maxWorkGroupSize> {};
template<> struct Map<CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS                    > : public MapBase<CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS,                    uint32_t,                        &DeviceInfo::maxWorkItemDimensions> {};
template<> struct Map<CL_DEVICE_MAX_WRITE_IMAGE_ARGS                        > : public MapBase<CL_DEVICE_MAX_WRITE_IMAGE_ARGS,                        uint32_t,                        &DeviceInfo::maxWriteImageArgs> {};
template<> struct Map<CL_DEVICE_MEM_BASE_ADDR_ALIGN                         > : public MapBase<CL_DEVICE_MEM_BASE_ADDR_ALIGN,                         uint32_t,                        &DeviceInfo::memBaseAddressAlign> {};
template<> struct Map<CL_DEVICE_ME_VERSION_INTEL                            > : public MapBase<CL_DEVICE_ME_VERSION_INTEL,                            uint32_t,                        &DeviceInfo::vmeVersion> {};
template<> struct Map<CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE                    > : public MapBase<CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE,                    uint32_t,                        &DeviceInfo::minDataTypeAlignSize> {};
template<> struct Map<CL_DEVICE_NAME                                        > : public MapBase<CL_DEVICE_NAME,                                        const char *,                    &DeviceInfo::name> {};
template<> struct Map<CL_DEVICE_NATIVE_VECTOR_WIDTH_CHAR                    > : public MapBase<CL_DEVICE_NATIVE_VECTOR_WIDTH_CHAR,                    uint32_t,                        &DeviceInfo::nativeVectorWidthChar> {};
template<> struct Map<CL_DEVICE_NATIVE_VECTOR_WIDTH_DOUBLE                  > : public MapBase<CL_DEVICE_NATIVE_VECTOR_WIDTH_DOUBLE,                  uint32_t,                        &DeviceInfo::nativeVectorWidthDouble> {};
template<> struct Map<CL_DEVICE_NATIVE_VECTOR_WIDTH_FLOAT                   > : public MapBase<CL_DEVICE_NATIVE_VECTOR_WIDTH_FLOAT,                   uint32_t,                        &DeviceInfo::nativeVectorWidthFloat> {};
template<> struct Map<CL_DEVICE_NATIVE_VECTOR_WIDTH_HALF                    > : public MapBase<CL_DEVICE_NATIVE_VECTOR_WIDTH_HALF,                    uint32_t,                        &DeviceInfo::nativeVectorWidthHalf> {};
template<> struct Map<CL_DEVICE_NATIVE_VECTOR_WIDTH_INT                     > : public MapBase<CL_DEVICE_NATIVE_VECTOR_WIDTH_INT,                     uint32_t,                        &DeviceInfo::nativeVectorWidthInt> {};
template<> struct Map<CL_DEVICE_NATIVE_VECTOR_WIDTH_LONG                    > : public MapBase<CL_DEVICE_NATIVE_VECTOR_WIDTH_LONG,                    uint32_t,                        &DeviceInfo::nativeVectorWidthLong> {};
template<> struct Map<CL_DEVICE_NATIVE_VECTOR_WIDTH_SHORT                   > : public MapBase<CL_DEVICE_NATIVE_VECTOR_WIDTH_SHORT,                   uint32_t,                        &DeviceInfo::nativeVectorWidthShort> {};
template<> struct Map<CL_DEVICE_OPENCL_C_VERSION                            > : public MapBase<CL_DEVICE_OPENCL_C_VERSION,                            const char *,                    &DeviceInfo::clCVersion> {};
template<> struct Map<CL_DEVICE_PARENT_DEVICE                               > : public MapBase<CL_DEVICE_PARENT_DEVICE,                               cl_device_id,                    &DeviceInfo::parentDevice> {};
template<> struct Map<CL_DEVICE_PARTITION_AFFINITY_DOMAIN                   > : public MapBase<CL_DEVICE_PARTITION_AFFINITY_DOMAIN,                   uint64_t,                        &DeviceInfo::partitionAffinityDomain> {};
template<> struct Map<CL_DEVICE_PARTITION_MAX_SUB_DEVICES                   > : public MapBase<CL_DEVICE_PARTITION_MAX_SUB_DEVICES,                   uint32_t,                        &DeviceInfo::partitionMaxSubDevices> {};
template<> struct Map<CL_DEVICE_PARTITION_PROPERTIES                        > : public MapBase<CL_DEVICE_PARTITION_PROPERTIES,                        cl_device_partition_property[2], &DeviceInfo::partitionProperties> {};
template<> struct Map<CL_DEVICE_PARTITION_TYPE                              > : public MapBase<CL_DEVICE_PARTITION_TYPE,                              cl_device_partition_property[3], &DeviceInfo::partitionType> {};
template<> struct Map<CL_DEVICE_PIPE_MAX_ACTIVE_RESERVATIONS                > : public MapBase<CL_DEVICE_PIPE_MAX_ACTIVE_RESERVATIONS,                uint32_t,                        &DeviceInfo::pipeMaxActiveReservations> {};
template<> struct Map<CL_DEVICE_PIPE_MAX_PACKET_SIZE                        > : public MapBase<CL_DEVICE_PIPE_MAX_PACKET_SIZE,                        uint32_t,                        &DeviceInfo::pipeMaxPacketSize> {};
template<> struct Map<CL_DEVICE_PLANAR_YUV_MAX_HEIGHT_INTEL                 > : public MapBase<CL_DEVICE_PLANAR_YUV_MAX_HEIGHT_INTEL,                 size_t,                          &DeviceInfo::planarYuvMaxHeight> {};
template<> struct Map<CL_DEVICE_PLANAR_YUV_MAX_WIDTH_INTEL                  > : public MapBase<CL_DEVICE_PLANAR_YUV_MAX_WIDTH_INTEL,                  size_t,                          &DeviceInfo::planarYuvMaxWidth> {};
template<> struct Map<CL_DEVICE_PREFERRED_GLOBAL_ATOMIC_ALIGNMENT           > : public MapBase<CL_DEVICE_PREFERRED_GLOBAL_ATOMIC_ALIGNMENT,           uint32_t,                        &DeviceInfo::preferredGlobalAtomicAlignment> {};
template<> struct Map<CL_DEVICE_PREFERRED_INTEROP_USER_SYNC                 > : public MapBase<CL_DEVICE_PREFERRED_INTEROP_USER_SYNC,                 uint32_t,                        &DeviceInfo::preferredInteropUserSync> {};
template<> struct Map<CL_DEVICE_PREFERRED_LOCAL_ATOMIC_ALIGNMENT            > : public MapBase<CL_DEVICE_PREFERRED_LOCAL_ATOMIC_ALIGNMENT,            uint32_t,                        &DeviceInfo::preferredLocalAtomicAlignment> {};
template<> struct Map<CL_DEVICE_PREFERRED_PLATFORM_ATOMIC_ALIGNMENT         > : public MapBase<CL_DEVICE_PREFERRED_PLATFORM_ATOMIC_ALIGNMENT,         uint32_t,                        &DeviceInfo::preferredPlatformAtomicAlignment> {};
template<> struct Map<CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR                 > : public MapBase<CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR,                 uint32_t,                        &DeviceInfo::preferredVectorWidthChar> {};
template<> struct Map<CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE               > : public MapBase<CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE,               uint32_t,                        &DeviceInfo::preferredVectorWidthDouble> {};
template<> struct Map<CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT                > : public MapBase<CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT,                uint32_t,                        &DeviceInfo::preferredVectorWidthFloat> {};
template<> struct Map<CL_DEVICE_PREFERRED_VECTOR_WIDTH_HALF                 > : public MapBase<CL_DEVICE_PREFERRED_VECTOR_WIDTH_HALF,                 uint32_t,                        &DeviceInfo::preferredVectorWidthHalf> {};
template<> struct Map<CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT                  > : public MapBase<CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT,                  uint32_t,                        &DeviceInfo::preferredVectorWidthInt> {};
template<> struct Map<CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG                 > : public MapBase<CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG,                 uint32_t,                        &DeviceInfo::preferredVectorWidthLong> {};
template<> struct Map<CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT                > : public MapBase<CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT,                uint32_t,                        &DeviceInfo::preferredVectorWidthShort> {};
template<> struct Map<CL_DEVICE_PRINTF_BUFFER_SIZE                          > : public MapBase<CL_DEVICE_PRINTF_BUFFER_SIZE,                          size_t,                          &DeviceInfo::printfBufferSize> {};
template<> struct Map<CL_DEVICE_PROFILE                                     > : public MapBase<CL_DEVICE_PROFILE,                                     const char *,                    &DeviceInfo::profile> {};
template<> struct Map<CL_DEVICE_PROFILING_TIMER_RESOLUTION                  > : public MapBase<CL_DEVICE_PROFILING_TIMER_RESOLUTION,                  size_t,                          &DeviceInfo::outProfilingTimerResolution> {};
template<> struct Map<CL_DEVICE_QUEUE_ON_DEVICE_MAX_SIZE                    > : public MapBase<CL_DEVICE_QUEUE_ON_DEVICE_MAX_SIZE,                    uint32_t,                        &DeviceInfo::queueOnDeviceMaxSize> {};
template<> struct Map<CL_DEVICE_QUEUE_ON_DEVICE_PREFERRED_SIZE              > : public MapBase<CL_DEVICE_QUEUE_ON_DEVICE_PREFERRED_SIZE,              uint32_t,                        &DeviceInfo::queueOnDevicePreferredSize> {};
template<> struct Map<CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES                  > : public MapBase<CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES,                  uint64_t,                        &DeviceInfo::queueOnDeviceProperties> {};
template<> struct Map<CL_DEVICE_QUEUE_ON_HOST_PROPERTIES                    > : public MapBase<CL_DEVICE_QUEUE_ON_HOST_PROPERTIES,                    uint64_t,                        &DeviceInfo::queueOnHostProperties> {};
template<> struct Map<CL_DEVICE_SHARED_SYSTEM_MEM_CAPABILITIES_INTEL        > : public MapBase<CL_DEVICE_SHARED_SYSTEM_MEM_CAPABILITIES_INTEL,        uint64_t,                        &DeviceInfo::sharedSystemMemCapabilities> {};
template<> struct Map<CL_DEVICE_SINGLE_DEVICE_SHARED_MEM_CAPABILITIES_INTEL > : public MapBase<CL_DEVICE_SINGLE_DEVICE_SHARED_MEM_CAPABILITIES_INTEL, uint64_t,                        &DeviceInfo::singleDeviceSharedMemCapabilities> {};
template<> struct Map<CL_DEVICE_SINGLE_FP_CONFIG                            > : public MapBase<CL_DEVICE_SINGLE_FP_CONFIG,                            uint64_t,                        &DeviceInfo::singleFpConfig> {};
template<> struct Map<CL_DEVICE_SLICE_COUNT_INTEL                           > : public MapBase<CL_DEVICE_SLICE_COUNT_INTEL,                           size_t,                          &DeviceInfo::maxSliceCount> {};
template<> struct Map<CL_DEVICE_SPIR_VERSIONS                               > : public MapBase<CL_DEVICE_SPIR_VERSIONS,                               const char *,                    &DeviceInfo::spirVersions> {};
template<> struct Map<CL_DEVICE_SUB_GROUP_INDEPENDENT_FORWARD_PROGRESS      > : public MapBase<CL_DEVICE_SUB_GROUP_INDEPENDENT_FORWARD_PROGRESS,      uint32_t,                        &DeviceInfo::independentForwardProgress> {};
template<> struct Map<CL_DEVICE_SVM_CAPABILITIES                            > : public MapBase<CL_DEVICE_SVM_CAPABILITIES,                            uint64_t,                        &DeviceInfo::svmCapabilities> {};
template<> struct Map<CL_DEVICE_TYPE                                        > : public MapBase<CL_DEVICE_TYPE,                                        uint64_t,                        &DeviceInfo::deviceType> {};
template<> struct Map<CL_DEVICE_VENDOR                                      > : public MapBase<CL_DEVICE_VENDOR,                                      const char *,                    &DeviceInfo::vendor> {};
template<> struct Map<CL_DEVICE_VENDOR_ID                                   > : public MapBase<CL_DEVICE_VENDOR_ID,                                   uint32_t,                        &DeviceInfo::vendorId> {};
template<> struct Map<CL_DEVICE_VERSION                                     > : public MapBase<CL_DEVICE_VERSION,                                     const char *,                    &DeviceInfo::clVersion> {};
template<> struct Map<CL_DRIVER_VERSION                                     > : public MapBase<CL_DRIVER_VERSION,                                     const char *,                    &DeviceInfo::driverVersion> {};
// clang-format on
} // namespace DeviceInfoTable
