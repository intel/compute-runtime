/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device/device_info.h"

#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/source/device/cl_device.h"
#include "opencl/source/device/cl_device_info.h"

#include "CL/cl_ext_intel.h"
#include <CL/cl.h>
#include <CL/cl_ext.h>

namespace NEO {

namespace ClDeviceInfoTable {

template <cl_device_info Param, typename _Type, _Type ClDeviceInfo::*val>
struct ClMapBase {
    enum { param = Param };
    typedef _Type Type;
    enum { size = sizeof(Type) };

    static const Type &getValue(const NEO::ClDevice &clDevice) {
        return clDevice.getDeviceInfo().*val;
    }
};

template <cl_device_info Param, typename _Type, _Type DeviceInfo::*val>
struct MapBase {
    enum { param = Param };
    typedef _Type Type;
    enum { size = sizeof(Type) };

    static const Type &getValue(const NEO::ClDevice &clDevice) {
        return clDevice.getSharedDeviceInfo().*val;
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

template<> struct Map<CL_DEVICE_ADDRESS_BITS                    > : public MapBase<CL_DEVICE_ADDRESS_BITS,                     unsigned int, &DeviceInfo::addressBits> {};
template<> struct Map<CL_DEVICE_AVC_ME_SUPPORTS_PREEMPTION_INTEL> : public MapBase<CL_DEVICE_AVC_ME_SUPPORTS_PREEMPTION_INTEL, uint32_t,     &DeviceInfo::vmeAvcSupportsPreemption> {};
template<> struct Map<CL_DEVICE_ERROR_CORRECTION_SUPPORT        > : public MapBase<CL_DEVICE_ERROR_CORRECTION_SUPPORT,         uint32_t,     &DeviceInfo::errorCorrectionSupport> {};
template<> struct Map<CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE       > : public MapBase<CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE,        unsigned int, &DeviceInfo::globalMemCachelineSize> {};
template<> struct Map<CL_DEVICE_GLOBAL_MEM_SIZE                 > : public MapBase<CL_DEVICE_GLOBAL_MEM_SIZE,                  uint64_t,     &DeviceInfo::globalMemSize> {};
template<> struct Map<CL_DEVICE_IL_VERSION                      > : public MapBase<CL_DEVICE_IL_VERSION,                       const char *, &DeviceInfo::ilVersion> {};
template<> struct Map<CL_DEVICE_IMAGE2D_MAX_HEIGHT              > : public MapBase<CL_DEVICE_IMAGE2D_MAX_HEIGHT,               size_t,       &DeviceInfo::image2DMaxHeight> {};
template<> struct Map<CL_DEVICE_IMAGE2D_MAX_WIDTH               > : public MapBase<CL_DEVICE_IMAGE2D_MAX_WIDTH,                size_t,       &DeviceInfo::image2DMaxWidth> {};
template<> struct Map<CL_DEVICE_IMAGE3D_MAX_DEPTH               > : public MapBase<CL_DEVICE_IMAGE3D_MAX_DEPTH,                size_t,       &DeviceInfo::image3DMaxDepth> {};
template<> struct Map<CL_DEVICE_IMAGE_MAX_ARRAY_SIZE            > : public MapBase<CL_DEVICE_IMAGE_MAX_ARRAY_SIZE,             size_t,       &DeviceInfo::imageMaxArraySize> {};
template<> struct Map<CL_DEVICE_IMAGE_MAX_BUFFER_SIZE           > : public MapBase<CL_DEVICE_IMAGE_MAX_BUFFER_SIZE,            size_t,       &DeviceInfo::imageMaxBufferSize> {};
template<> struct Map<CL_DEVICE_IMAGE_SUPPORT                   > : public MapBase<CL_DEVICE_IMAGE_SUPPORT,                    uint32_t,     &DeviceInfo::imageSupport> {};
template<> struct Map<CL_DEVICE_LOCAL_MEM_SIZE                  > : public MapBase<CL_DEVICE_LOCAL_MEM_SIZE,                   uint64_t,     &DeviceInfo::localMemSize> {};
template<> struct Map<CL_DEVICE_MAX_CLOCK_FREQUENCY             > : public MapBase<CL_DEVICE_MAX_CLOCK_FREQUENCY,              uint32_t,     &DeviceInfo::maxClockFrequency> {};
template<> struct Map<CL_DEVICE_MAX_MEM_ALLOC_SIZE              > : public MapBase<CL_DEVICE_MAX_MEM_ALLOC_SIZE,               uint64_t,     &DeviceInfo::maxMemAllocSize> {};
template<> struct Map<CL_DEVICE_MAX_ON_DEVICE_QUEUES            > : public MapBase<CL_DEVICE_MAX_ON_DEVICE_QUEUES,             uint32_t,     &DeviceInfo::maxOnDeviceQueues> {};
template<> struct Map<CL_DEVICE_MAX_PARAMETER_SIZE              > : public MapBase<CL_DEVICE_MAX_PARAMETER_SIZE,               size_t,       &DeviceInfo::maxParameterSize> {};
template<> struct Map<CL_DEVICE_MAX_READ_IMAGE_ARGS             > : public MapBase<CL_DEVICE_MAX_READ_IMAGE_ARGS,              uint32_t,     &DeviceInfo::maxReadImageArgs> {};
template<> struct Map<CL_DEVICE_MAX_SAMPLERS                    > : public MapBase<CL_DEVICE_MAX_SAMPLERS,                     uint32_t,     &DeviceInfo::maxSamplers> {};
template<> struct Map<CL_DEVICE_MAX_WORK_GROUP_SIZE             > : public MapBase<CL_DEVICE_MAX_WORK_GROUP_SIZE,              size_t,       &DeviceInfo::maxWorkGroupSize> {};
template<> struct Map<CL_DEVICE_MAX_WRITE_IMAGE_ARGS            > : public MapBase<CL_DEVICE_MAX_WRITE_IMAGE_ARGS,             uint32_t,     &DeviceInfo::maxWriteImageArgs> {};
template<> struct Map<CL_DEVICE_PRINTF_BUFFER_SIZE              > : public MapBase<CL_DEVICE_PRINTF_BUFFER_SIZE,               size_t,       &DeviceInfo::printfBufferSize> {};
template<> struct Map<CL_DEVICE_PROFILING_TIMER_RESOLUTION      > : public MapBase<CL_DEVICE_PROFILING_TIMER_RESOLUTION,       size_t,       &DeviceInfo::outProfilingTimerResolution> {};
template<> struct Map<CL_DEVICE_VENDOR_ID                       > : public MapBase<CL_DEVICE_VENDOR_ID,                        uint32_t,     &DeviceInfo::vendorId> {};

template<> struct Map<CL_DEVICE_AVAILABLE                                   > : public ClMapBase<CL_DEVICE_AVAILABLE,                                   uint32_t,                        &ClDeviceInfo::deviceAvailable> {};
template<> struct Map<CL_DEVICE_AVC_ME_SUPPORTS_TEXTURE_SAMPLER_USE_INTEL   > : public ClMapBase<CL_DEVICE_AVC_ME_SUPPORTS_TEXTURE_SAMPLER_USE_INTEL,   uint32_t,                        &ClDeviceInfo::vmeAvcSupportsTextureSampler> {};
template<> struct Map<CL_DEVICE_AVC_ME_VERSION_INTEL                        > : public ClMapBase<CL_DEVICE_AVC_ME_VERSION_INTEL,                        uint32_t,                        &ClDeviceInfo::vmeAvcVersion> {};
template<> struct Map<CL_DEVICE_BUILT_IN_KERNELS                            > : public ClMapBase<CL_DEVICE_BUILT_IN_KERNELS,                            const char *,                    &ClDeviceInfo::builtInKernels> {};
template<> struct Map<CL_DEVICE_COMPILER_AVAILABLE                          > : public ClMapBase<CL_DEVICE_COMPILER_AVAILABLE,                          uint32_t,                        &ClDeviceInfo::compilerAvailable> {};
template<> struct Map<CL_DEVICE_CROSS_DEVICE_SHARED_MEM_CAPABILITIES_INTEL  > : public ClMapBase<CL_DEVICE_CROSS_DEVICE_SHARED_MEM_CAPABILITIES_INTEL,  uint64_t,                        &ClDeviceInfo::crossDeviceSharedMemCapabilities> {};
template<> struct Map<CL_DEVICE_DEVICE_MEM_CAPABILITIES_INTEL               > : public ClMapBase<CL_DEVICE_DEVICE_MEM_CAPABILITIES_INTEL,               uint64_t,                        &ClDeviceInfo::deviceMemCapabilities> {};
template<> struct Map<CL_DEVICE_DOUBLE_FP_CONFIG                            > : public ClMapBase<CL_DEVICE_DOUBLE_FP_CONFIG,                            uint64_t,                        &ClDeviceInfo::doubleFpConfig> {};
template<> struct Map<CL_DEVICE_DRIVER_VERSION_INTEL                        > : public ClMapBase<CL_DEVICE_DRIVER_VERSION_INTEL,                        uint32_t,                        &ClDeviceInfo::internalDriverVersion> {};
template<> struct Map<CL_DEVICE_ENDIAN_LITTLE                               > : public ClMapBase<CL_DEVICE_ENDIAN_LITTLE,                               uint32_t,                        &ClDeviceInfo::endianLittle> {};
template<> struct Map<CL_DEVICE_EXECUTION_CAPABILITIES                      > : public ClMapBase<CL_DEVICE_EXECUTION_CAPABILITIES,                      uint64_t,                        &ClDeviceInfo::executionCapabilities> {};
template<> struct Map<CL_DEVICE_EXTENSIONS                                  > : public ClMapBase<CL_DEVICE_EXTENSIONS,                                  const char *,                    &ClDeviceInfo::deviceExtensions> {};
template<> struct Map<CL_DEVICE_GLOBAL_MEM_CACHE_SIZE                       > : public ClMapBase<CL_DEVICE_GLOBAL_MEM_CACHE_SIZE,                       uint64_t,                        &ClDeviceInfo::globalMemCacheSize> {};
template<> struct Map<CL_DEVICE_GLOBAL_MEM_CACHE_TYPE                       > : public ClMapBase<CL_DEVICE_GLOBAL_MEM_CACHE_TYPE,                       uint32_t,                        &ClDeviceInfo::globalMemCacheType> {};
template<> struct Map<CL_DEVICE_GLOBAL_VARIABLE_PREFERRED_TOTAL_SIZE        > : public ClMapBase<CL_DEVICE_GLOBAL_VARIABLE_PREFERRED_TOTAL_SIZE,        size_t,                          &ClDeviceInfo::globalVariablePreferredTotalSize> {};
template<> struct Map<CL_DEVICE_HALF_FP_CONFIG                              > : public ClMapBase<CL_DEVICE_HALF_FP_CONFIG,                              uint64_t,                        &ClDeviceInfo::halfFpConfig> {};
template<> struct Map<CL_DEVICE_HOST_MEM_CAPABILITIES_INTEL                 > : public ClMapBase<CL_DEVICE_HOST_MEM_CAPABILITIES_INTEL,                 uint64_t,                        &ClDeviceInfo::hostMemCapabilities> {};
template<> struct Map<CL_DEVICE_HOST_UNIFIED_MEMORY                         > : public ClMapBase<CL_DEVICE_HOST_UNIFIED_MEMORY,                         uint32_t,                        &ClDeviceInfo::hostUnifiedMemory> {};
template<> struct Map<CL_DEVICE_IMAGE3D_MAX_HEIGHT                          > : public ClMapBase<CL_DEVICE_IMAGE3D_MAX_HEIGHT,                          size_t,                          &ClDeviceInfo::image3DMaxHeight> {};
template<> struct Map<CL_DEVICE_IMAGE3D_MAX_WIDTH                           > : public ClMapBase<CL_DEVICE_IMAGE3D_MAX_WIDTH,                           size_t,                          &ClDeviceInfo::image3DMaxWidth> {};
template<> struct Map<CL_DEVICE_IMAGE_BASE_ADDRESS_ALIGNMENT                > : public ClMapBase<CL_DEVICE_IMAGE_BASE_ADDRESS_ALIGNMENT,                uint32_t,                        &ClDeviceInfo::imageBaseAddressAlignment> {};
template<> struct Map<CL_DEVICE_IMAGE_PITCH_ALIGNMENT                       > : public ClMapBase<CL_DEVICE_IMAGE_PITCH_ALIGNMENT,                       uint32_t,                        &ClDeviceInfo::imagePitchAlignment> {};
template<> struct Map<CL_DEVICE_LINKER_AVAILABLE                            > : public ClMapBase<CL_DEVICE_LINKER_AVAILABLE,                            uint32_t,                        &ClDeviceInfo::linkerAvailable> {};
template<> struct Map<CL_DEVICE_LOCAL_MEM_TYPE                              > : public ClMapBase<CL_DEVICE_LOCAL_MEM_TYPE,                              uint32_t,                        &ClDeviceInfo::localMemType> {};
template<> struct Map<CL_DEVICE_MAX_COMPUTE_UNITS                           > : public ClMapBase<CL_DEVICE_MAX_COMPUTE_UNITS,                           uint32_t,                        &ClDeviceInfo::maxComputUnits> {};
template<> struct Map<CL_DEVICE_MAX_CONSTANT_ARGS                           > : public ClMapBase<CL_DEVICE_MAX_CONSTANT_ARGS,                           uint32_t,                        &ClDeviceInfo::maxConstantArgs> {};
template<> struct Map<CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE                    > : public ClMapBase<CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE,                    uint64_t,                        &ClDeviceInfo::maxConstantBufferSize> {};
template<> struct Map<CL_DEVICE_MAX_GLOBAL_VARIABLE_SIZE                    > : public ClMapBase<CL_DEVICE_MAX_GLOBAL_VARIABLE_SIZE,                    size_t,                          &ClDeviceInfo::maxGlobalVariableSize> {};
template<> struct Map<CL_DEVICE_MAX_NUM_SUB_GROUPS                          > : public ClMapBase<CL_DEVICE_MAX_NUM_SUB_GROUPS,                          uint32_t,                        &ClDeviceInfo::maxNumOfSubGroups> {};
template<> struct Map<CL_DEVICE_MAX_ON_DEVICE_EVENTS                        > : public ClMapBase<CL_DEVICE_MAX_ON_DEVICE_EVENTS,                        uint32_t,                        &ClDeviceInfo::maxOnDeviceEvents> {};
template<> struct Map<CL_DEVICE_MAX_PIPE_ARGS                               > : public ClMapBase<CL_DEVICE_MAX_PIPE_ARGS,                               uint32_t,                        &ClDeviceInfo::maxPipeArgs> {};
template<> struct Map<CL_DEVICE_MAX_READ_WRITE_IMAGE_ARGS                   > : public ClMapBase<CL_DEVICE_MAX_READ_WRITE_IMAGE_ARGS,                   uint32_t,                        &ClDeviceInfo::maxReadWriteImageArgs> {};
template<> struct Map<CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS                    > : public ClMapBase<CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS,                    uint32_t,                        &ClDeviceInfo::maxWorkItemDimensions> {};
template<> struct Map<CL_DEVICE_MEM_BASE_ADDR_ALIGN                         > : public ClMapBase<CL_DEVICE_MEM_BASE_ADDR_ALIGN,                         uint32_t,                        &ClDeviceInfo::memBaseAddressAlign> {};
template<> struct Map<CL_DEVICE_ME_VERSION_INTEL                            > : public ClMapBase<CL_DEVICE_ME_VERSION_INTEL,                            uint32_t,                        &ClDeviceInfo::vmeVersion> {};
template<> struct Map<CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE                    > : public ClMapBase<CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE,                    uint32_t,                        &ClDeviceInfo::minDataTypeAlignSize> {};
template<> struct Map<CL_DEVICE_NAME                                        > : public ClMapBase<CL_DEVICE_NAME,                                        const char *,                    &ClDeviceInfo::name> {};
template<> struct Map<CL_DEVICE_NATIVE_VECTOR_WIDTH_CHAR                    > : public ClMapBase<CL_DEVICE_NATIVE_VECTOR_WIDTH_CHAR,                    uint32_t,                        &ClDeviceInfo::nativeVectorWidthChar> {};
template<> struct Map<CL_DEVICE_NATIVE_VECTOR_WIDTH_DOUBLE                  > : public ClMapBase<CL_DEVICE_NATIVE_VECTOR_WIDTH_DOUBLE,                  uint32_t,                        &ClDeviceInfo::nativeVectorWidthDouble> {};
template<> struct Map<CL_DEVICE_NATIVE_VECTOR_WIDTH_FLOAT                   > : public ClMapBase<CL_DEVICE_NATIVE_VECTOR_WIDTH_FLOAT,                   uint32_t,                        &ClDeviceInfo::nativeVectorWidthFloat> {};
template<> struct Map<CL_DEVICE_NATIVE_VECTOR_WIDTH_HALF                    > : public ClMapBase<CL_DEVICE_NATIVE_VECTOR_WIDTH_HALF,                    uint32_t,                        &ClDeviceInfo::nativeVectorWidthHalf> {};
template<> struct Map<CL_DEVICE_NATIVE_VECTOR_WIDTH_INT                     > : public ClMapBase<CL_DEVICE_NATIVE_VECTOR_WIDTH_INT,                     uint32_t,                        &ClDeviceInfo::nativeVectorWidthInt> {};
template<> struct Map<CL_DEVICE_NATIVE_VECTOR_WIDTH_LONG                    > : public ClMapBase<CL_DEVICE_NATIVE_VECTOR_WIDTH_LONG,                    uint32_t,                        &ClDeviceInfo::nativeVectorWidthLong> {};
template<> struct Map<CL_DEVICE_NATIVE_VECTOR_WIDTH_SHORT                   > : public ClMapBase<CL_DEVICE_NATIVE_VECTOR_WIDTH_SHORT,                   uint32_t,                        &ClDeviceInfo::nativeVectorWidthShort> {};
template<> struct Map<CL_DEVICE_OPENCL_C_VERSION                            > : public ClMapBase<CL_DEVICE_OPENCL_C_VERSION,                            const char *,                    &ClDeviceInfo::clCVersion> {};
template<> struct Map<CL_DEVICE_PARENT_DEVICE                               > : public ClMapBase<CL_DEVICE_PARENT_DEVICE,                               cl_device_id,                    &ClDeviceInfo::parentDevice> {};
template<> struct Map<CL_DEVICE_PARTITION_AFFINITY_DOMAIN                   > : public ClMapBase<CL_DEVICE_PARTITION_AFFINITY_DOMAIN,                   uint64_t,                        &ClDeviceInfo::partitionAffinityDomain> {};
template<> struct Map<CL_DEVICE_PARTITION_MAX_SUB_DEVICES                   > : public ClMapBase<CL_DEVICE_PARTITION_MAX_SUB_DEVICES,                   uint32_t,                        &ClDeviceInfo::partitionMaxSubDevices> {};
template<> struct Map<CL_DEVICE_PARTITION_PROPERTIES                        > : public ClMapBase<CL_DEVICE_PARTITION_PROPERTIES,                        cl_device_partition_property[2], &ClDeviceInfo::partitionProperties> {};
template<> struct Map<CL_DEVICE_PARTITION_TYPE                              > : public ClMapBase<CL_DEVICE_PARTITION_TYPE,                              cl_device_partition_property[3], &ClDeviceInfo::partitionType> {};
template<> struct Map<CL_DEVICE_PIPE_MAX_ACTIVE_RESERVATIONS                > : public ClMapBase<CL_DEVICE_PIPE_MAX_ACTIVE_RESERVATIONS,                uint32_t,                        &ClDeviceInfo::pipeMaxActiveReservations> {};
template<> struct Map<CL_DEVICE_PIPE_MAX_PACKET_SIZE                        > : public ClMapBase<CL_DEVICE_PIPE_MAX_PACKET_SIZE,                        uint32_t,                        &ClDeviceInfo::pipeMaxPacketSize> {};
template<> struct Map<CL_DEVICE_PLANAR_YUV_MAX_HEIGHT_INTEL                 > : public ClMapBase<CL_DEVICE_PLANAR_YUV_MAX_HEIGHT_INTEL,                 size_t,                          &ClDeviceInfo::planarYuvMaxHeight> {};
template<> struct Map<CL_DEVICE_PLANAR_YUV_MAX_WIDTH_INTEL                  > : public ClMapBase<CL_DEVICE_PLANAR_YUV_MAX_WIDTH_INTEL,                  size_t,                          &ClDeviceInfo::planarYuvMaxWidth> {};
template<> struct Map<CL_DEVICE_PREFERRED_GLOBAL_ATOMIC_ALIGNMENT           > : public ClMapBase<CL_DEVICE_PREFERRED_GLOBAL_ATOMIC_ALIGNMENT,           uint32_t,                        &ClDeviceInfo::preferredGlobalAtomicAlignment> {};
template<> struct Map<CL_DEVICE_PREFERRED_INTEROP_USER_SYNC                 > : public ClMapBase<CL_DEVICE_PREFERRED_INTEROP_USER_SYNC,                 uint32_t,                        &ClDeviceInfo::preferredInteropUserSync> {};
template<> struct Map<CL_DEVICE_PREFERRED_LOCAL_ATOMIC_ALIGNMENT            > : public ClMapBase<CL_DEVICE_PREFERRED_LOCAL_ATOMIC_ALIGNMENT,            uint32_t,                        &ClDeviceInfo::preferredLocalAtomicAlignment> {};
template<> struct Map<CL_DEVICE_PREFERRED_PLATFORM_ATOMIC_ALIGNMENT         > : public ClMapBase<CL_DEVICE_PREFERRED_PLATFORM_ATOMIC_ALIGNMENT,         uint32_t,                        &ClDeviceInfo::preferredPlatformAtomicAlignment> {};
template<> struct Map<CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR                 > : public ClMapBase<CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR,                 uint32_t,                        &ClDeviceInfo::preferredVectorWidthChar> {};
template<> struct Map<CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE               > : public ClMapBase<CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE,               uint32_t,                        &ClDeviceInfo::preferredVectorWidthDouble> {};
template<> struct Map<CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT                > : public ClMapBase<CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT,                uint32_t,                        &ClDeviceInfo::preferredVectorWidthFloat> {};
template<> struct Map<CL_DEVICE_PREFERRED_VECTOR_WIDTH_HALF                 > : public ClMapBase<CL_DEVICE_PREFERRED_VECTOR_WIDTH_HALF,                 uint32_t,                        &ClDeviceInfo::preferredVectorWidthHalf> {};
template<> struct Map<CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT                  > : public ClMapBase<CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT,                  uint32_t,                        &ClDeviceInfo::preferredVectorWidthInt> {};
template<> struct Map<CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG                 > : public ClMapBase<CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG,                 uint32_t,                        &ClDeviceInfo::preferredVectorWidthLong> {};
template<> struct Map<CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT                > : public ClMapBase<CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT,                uint32_t,                        &ClDeviceInfo::preferredVectorWidthShort> {};
template<> struct Map<CL_DEVICE_PROFILE                                     > : public ClMapBase<CL_DEVICE_PROFILE,                                     const char *,                    &ClDeviceInfo::profile> {};
template<> struct Map<CL_DEVICE_QUEUE_ON_DEVICE_MAX_SIZE                    > : public ClMapBase<CL_DEVICE_QUEUE_ON_DEVICE_MAX_SIZE,                    uint32_t,                        &ClDeviceInfo::queueOnDeviceMaxSize> {};
template<> struct Map<CL_DEVICE_QUEUE_ON_DEVICE_PREFERRED_SIZE              > : public ClMapBase<CL_DEVICE_QUEUE_ON_DEVICE_PREFERRED_SIZE,              uint32_t,                        &ClDeviceInfo::queueOnDevicePreferredSize> {};
template<> struct Map<CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES                  > : public ClMapBase<CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES,                  uint64_t,                        &ClDeviceInfo::queueOnDeviceProperties> {};
template<> struct Map<CL_DEVICE_QUEUE_ON_HOST_PROPERTIES                    > : public ClMapBase<CL_DEVICE_QUEUE_ON_HOST_PROPERTIES,                    uint64_t,                        &ClDeviceInfo::queueOnHostProperties> {};
template<> struct Map<CL_DEVICE_SHARED_SYSTEM_MEM_CAPABILITIES_INTEL        > : public ClMapBase<CL_DEVICE_SHARED_SYSTEM_MEM_CAPABILITIES_INTEL,        uint64_t,                        &ClDeviceInfo::sharedSystemMemCapabilities> {};
template<> struct Map<CL_DEVICE_SINGLE_DEVICE_SHARED_MEM_CAPABILITIES_INTEL > : public ClMapBase<CL_DEVICE_SINGLE_DEVICE_SHARED_MEM_CAPABILITIES_INTEL, uint64_t,                        &ClDeviceInfo::singleDeviceSharedMemCapabilities> {};
template<> struct Map<CL_DEVICE_SINGLE_FP_CONFIG                            > : public ClMapBase<CL_DEVICE_SINGLE_FP_CONFIG,                            uint64_t,                        &ClDeviceInfo::singleFpConfig> {};
template<> struct Map<CL_DEVICE_SLICE_COUNT_INTEL                           > : public ClMapBase<CL_DEVICE_SLICE_COUNT_INTEL,                           size_t,                          &ClDeviceInfo::maxSliceCount> {};
template<> struct Map<CL_DEVICE_SPIR_VERSIONS                               > : public ClMapBase<CL_DEVICE_SPIR_VERSIONS,                               const char *,                    &ClDeviceInfo::spirVersions> {};
template<> struct Map<CL_DEVICE_SUB_GROUP_INDEPENDENT_FORWARD_PROGRESS      > : public ClMapBase<CL_DEVICE_SUB_GROUP_INDEPENDENT_FORWARD_PROGRESS,      uint32_t,                        &ClDeviceInfo::independentForwardProgress> {};
template<> struct Map<CL_DEVICE_SVM_CAPABILITIES                            > : public ClMapBase<CL_DEVICE_SVM_CAPABILITIES,                            uint64_t,                        &ClDeviceInfo::svmCapabilities> {};
template<> struct Map<CL_DEVICE_TYPE                                        > : public ClMapBase<CL_DEVICE_TYPE,                                        uint64_t,                        &ClDeviceInfo::deviceType> {};
template<> struct Map<CL_DEVICE_VENDOR                                      > : public ClMapBase<CL_DEVICE_VENDOR,                                      const char *,                    &ClDeviceInfo::vendor> {};
template<> struct Map<CL_DEVICE_VERSION                                     > : public ClMapBase<CL_DEVICE_VERSION,                                     const char *,                    &ClDeviceInfo::clVersion> {};
template<> struct Map<CL_DRIVER_VERSION                                     > : public ClMapBase<CL_DRIVER_VERSION,                                     const char *,                    &ClDeviceInfo::driverVersion> {};
// clang-format on

} // namespace ClDeviceInfoTable

} // namespace NEO
