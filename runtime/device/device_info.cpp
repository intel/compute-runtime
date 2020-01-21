/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/device/device_info.h"

#include "core/helpers/get_info.h"
#include "core/os_interface/os_time.h"
#include "runtime/device/device.h"
#include "runtime/device/device_info_map.h"
#include "runtime/device/device_vector.h"
#include "runtime/helpers/device_helpers.h"
#include "runtime/platform/platform.h"

using DeviceInfoTable::Map;

namespace NEO {

template <cl_device_info Param>
inline void ClDevice::getStr(const void *&src,
                             size_t &size,
                             size_t &retSize) {
    src = Map<Param>::getValue(device.getDeviceInfo());
    retSize = size = strlen(Map<Param>::getValue(device.getDeviceInfo())) + 1;
}

template <>
inline void ClDevice::getCap<CL_DEVICE_MAX_WORK_ITEM_SIZES>(const void *&src,
                                                            size_t &size,
                                                            size_t &retSize) {
    src = device.getDeviceInfo().maxWorkItemSizes;
    retSize = size = sizeof(device.getDeviceInfo().maxWorkItemSizes);
}

template <>
inline void ClDevice::getCap<CL_DEVICE_PARTITION_PROPERTIES>(const void *&src,
                                                             size_t &size,
                                                             size_t &retSize) {
    static cl_device_partition_property property = 0;
    src = &property;
    retSize = size = sizeof(cl_device_partition_property *);
}

template <>
inline void ClDevice::getCap<CL_DEVICE_PLATFORM>(const void *&src,
                                                 size_t &size,
                                                 size_t &retSize) {
    src = &platformId;
    retSize = size = sizeof(cl_platform_id);
}

template <>
inline void ClDevice::getCap<CL_DEVICE_SUB_GROUP_SIZES_INTEL>(const void *&src,
                                                              size_t &size,
                                                              size_t &retSize) {
    src = device.getDeviceInfo().maxSubGroups;
    retSize = size = sizeof(device.getDeviceInfo().maxSubGroups);
}

cl_int ClDevice::getDeviceInfo(cl_device_info paramName,
                               size_t paramValueSize,
                               void *paramValue,
                               size_t *paramValueSizeRet) {
    cl_int retVal = CL_INVALID_VALUE;
    size_t srcSize = 0;
    size_t retSize = 0;
    cl_uint param;
    const void *src = nullptr;

    // clang-format off
    // please keep alphabetical order
    switch (paramName) {
    case CL_DEVICE_ADDRESS_BITS:                                getCap<CL_DEVICE_ADDRESS_BITS                                >(src, srcSize, retSize); break;
    case CL_DEVICE_AVAILABLE:                                   getCap<CL_DEVICE_AVAILABLE                                   >(src, srcSize, retSize); break;
    case CL_DEVICE_AVC_ME_SUPPORTS_PREEMPTION_INTEL:            getCap<CL_DEVICE_AVC_ME_SUPPORTS_PREEMPTION_INTEL            >(src, srcSize, retSize); break;
    case CL_DEVICE_AVC_ME_SUPPORTS_TEXTURE_SAMPLER_USE_INTEL:   getCap<CL_DEVICE_AVC_ME_SUPPORTS_TEXTURE_SAMPLER_USE_INTEL   >(src, srcSize, retSize); break;
    case CL_DEVICE_AVC_ME_VERSION_INTEL:                        getCap<CL_DEVICE_AVC_ME_VERSION_INTEL                        >(src, srcSize, retSize); break;
    case CL_DEVICE_BUILT_IN_KERNELS:                            getStr<CL_DEVICE_BUILT_IN_KERNELS                            >(src, srcSize, retSize); break;
    case CL_DEVICE_COMPILER_AVAILABLE:                          getCap<CL_DEVICE_COMPILER_AVAILABLE                          >(src, srcSize, retSize); break;
    case CL_DEVICE_CROSS_DEVICE_SHARED_MEM_CAPABILITIES_INTEL:  getCap<CL_DEVICE_CROSS_DEVICE_SHARED_MEM_CAPABILITIES_INTEL  >(src, srcSize, retSize); break;
    case CL_DEVICE_DEVICE_MEM_CAPABILITIES_INTEL:               getCap<CL_DEVICE_DEVICE_MEM_CAPABILITIES_INTEL               >(src, srcSize, retSize); break;
    case CL_DEVICE_DOUBLE_FP_CONFIG:                            getCap<CL_DEVICE_DOUBLE_FP_CONFIG                            >(src, srcSize, retSize); break;
    case CL_DEVICE_DRIVER_VERSION_INTEL:                        getCap<CL_DEVICE_DRIVER_VERSION_INTEL                        >(src, srcSize, retSize); break;
    case CL_DEVICE_ENDIAN_LITTLE:                               getCap<CL_DEVICE_ENDIAN_LITTLE                               >(src, srcSize, retSize); break;
    case CL_DEVICE_ERROR_CORRECTION_SUPPORT:                    getCap<CL_DEVICE_ERROR_CORRECTION_SUPPORT                    >(src, srcSize, retSize); break;
    case CL_DEVICE_EXECUTION_CAPABILITIES:                      getCap<CL_DEVICE_EXECUTION_CAPABILITIES                      >(src, srcSize, retSize); break;
    case CL_DEVICE_EXTENSIONS:                                  getStr<CL_DEVICE_EXTENSIONS                                  >(src, srcSize, retSize); break;
    case CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE:                   getCap<CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE                   >(src, srcSize, retSize); break;
    case CL_DEVICE_GLOBAL_MEM_CACHE_SIZE:                       getCap<CL_DEVICE_GLOBAL_MEM_CACHE_SIZE                       >(src, srcSize, retSize); break;
    case CL_DEVICE_GLOBAL_MEM_CACHE_TYPE:                       getCap<CL_DEVICE_GLOBAL_MEM_CACHE_TYPE                       >(src, srcSize, retSize); break;
    case CL_DEVICE_GLOBAL_MEM_SIZE:                             getCap<CL_DEVICE_GLOBAL_MEM_SIZE                             >(src, srcSize, retSize); break;
    case CL_DEVICE_GLOBAL_VARIABLE_PREFERRED_TOTAL_SIZE:        getCap<CL_DEVICE_GLOBAL_VARIABLE_PREFERRED_TOTAL_SIZE        >(src, srcSize, retSize); break;
    case CL_DEVICE_HALF_FP_CONFIG:                              getCap<CL_DEVICE_HALF_FP_CONFIG                              >(src, srcSize, retSize); break;
    case CL_DEVICE_HOST_MEM_CAPABILITIES_INTEL:                 getCap<CL_DEVICE_HOST_MEM_CAPABILITIES_INTEL                 >(src, srcSize, retSize); break;
    case CL_DEVICE_HOST_UNIFIED_MEMORY:                         getCap<CL_DEVICE_HOST_UNIFIED_MEMORY                         >(src, srcSize, retSize); break;
    case CL_DEVICE_IL_VERSION:                                  getStr<CL_DEVICE_IL_VERSION                                  >(src, srcSize, retSize); break;
    case CL_DEVICE_IMAGE_SUPPORT:                               getCap<CL_DEVICE_IMAGE_SUPPORT                               >(src, srcSize, retSize); break;
    case CL_DEVICE_LINKER_AVAILABLE:                            getCap<CL_DEVICE_LINKER_AVAILABLE                            >(src, srcSize, retSize); break;
    case CL_DEVICE_LOCAL_MEM_SIZE:                              getCap<CL_DEVICE_LOCAL_MEM_SIZE                              >(src, srcSize, retSize); break;
    case CL_DEVICE_LOCAL_MEM_TYPE:                              getCap<CL_DEVICE_LOCAL_MEM_TYPE                              >(src, srcSize, retSize); break;
    case CL_DEVICE_MAX_CLOCK_FREQUENCY:                         getCap<CL_DEVICE_MAX_CLOCK_FREQUENCY                         >(src, srcSize, retSize); break;
    case CL_DEVICE_MAX_COMPUTE_UNITS:                           getCap<CL_DEVICE_MAX_COMPUTE_UNITS                           >(src, srcSize, retSize); break;
    case CL_DEVICE_MAX_CONSTANT_ARGS:                           getCap<CL_DEVICE_MAX_CONSTANT_ARGS                           >(src, srcSize, retSize); break;
    case CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE:                    getCap<CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE                    >(src, srcSize, retSize); break;
    case CL_DEVICE_MAX_GLOBAL_VARIABLE_SIZE:                    getCap<CL_DEVICE_MAX_GLOBAL_VARIABLE_SIZE                    >(src, srcSize, retSize); break;
    case CL_DEVICE_MAX_MEM_ALLOC_SIZE:                          getCap<CL_DEVICE_MAX_MEM_ALLOC_SIZE                          >(src, srcSize, retSize); break;
    case CL_DEVICE_MAX_NUM_SUB_GROUPS:                          getCap<CL_DEVICE_MAX_NUM_SUB_GROUPS                          >(src, srcSize, retSize); break;
    case CL_DEVICE_MAX_ON_DEVICE_EVENTS:                        getCap<CL_DEVICE_MAX_ON_DEVICE_EVENTS                        >(src, srcSize, retSize); break;
    case CL_DEVICE_MAX_ON_DEVICE_QUEUES:                        getCap<CL_DEVICE_MAX_ON_DEVICE_QUEUES                        >(src, srcSize, retSize); break;
    case CL_DEVICE_MAX_PARAMETER_SIZE:                          getCap<CL_DEVICE_MAX_PARAMETER_SIZE                          >(src, srcSize, retSize); break;
    case CL_DEVICE_MAX_PIPE_ARGS:                               getCap<CL_DEVICE_MAX_PIPE_ARGS                               >(src, srcSize, retSize); break;
    case CL_DEVICE_MAX_SAMPLERS:                                getCap<CL_DEVICE_MAX_SAMPLERS                                >(src, srcSize, retSize); break;
    case CL_DEVICE_MAX_WORK_GROUP_SIZE:                         getCap<CL_DEVICE_MAX_WORK_GROUP_SIZE                         >(src, srcSize, retSize); break;
    case CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS:                    getCap<CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS                    >(src, srcSize, retSize); break;
    case CL_DEVICE_MAX_WORK_ITEM_SIZES:                         getCap<CL_DEVICE_MAX_WORK_ITEM_SIZES                         >(src, srcSize, retSize); break;
    case CL_DEVICE_MEM_BASE_ADDR_ALIGN:                         getCap<CL_DEVICE_MEM_BASE_ADDR_ALIGN                         >(src, srcSize, retSize); break;
    case CL_DEVICE_ME_VERSION_INTEL:                            getCap<CL_DEVICE_ME_VERSION_INTEL                            >(src, srcSize, retSize); break;
    case CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE:                    getCap<CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE                    >(src, srcSize, retSize); break;
    case CL_DEVICE_NAME:                                        getStr<CL_DEVICE_NAME                                        >(src, srcSize, retSize); break;
    case CL_DEVICE_NATIVE_VECTOR_WIDTH_CHAR:                    getCap<CL_DEVICE_NATIVE_VECTOR_WIDTH_CHAR                    >(src, srcSize, retSize); break;
    case CL_DEVICE_NATIVE_VECTOR_WIDTH_DOUBLE:                  getCap<CL_DEVICE_NATIVE_VECTOR_WIDTH_DOUBLE                  >(src, srcSize, retSize); break;
    case CL_DEVICE_NATIVE_VECTOR_WIDTH_FLOAT:                   getCap<CL_DEVICE_NATIVE_VECTOR_WIDTH_FLOAT                   >(src, srcSize, retSize); break;
    case CL_DEVICE_NATIVE_VECTOR_WIDTH_HALF:                    getCap<CL_DEVICE_NATIVE_VECTOR_WIDTH_HALF                    >(src, srcSize, retSize); break;
    case CL_DEVICE_NATIVE_VECTOR_WIDTH_INT:                     getCap<CL_DEVICE_NATIVE_VECTOR_WIDTH_INT                     >(src, srcSize, retSize); break;
    case CL_DEVICE_NATIVE_VECTOR_WIDTH_LONG:                    getCap<CL_DEVICE_NATIVE_VECTOR_WIDTH_LONG                    >(src, srcSize, retSize); break;
    case CL_DEVICE_NATIVE_VECTOR_WIDTH_SHORT:                   getCap<CL_DEVICE_NATIVE_VECTOR_WIDTH_SHORT                   >(src, srcSize, retSize); break;
    case CL_DEVICE_OPENCL_C_VERSION:                            getStr<CL_DEVICE_OPENCL_C_VERSION                            >(src, srcSize, retSize); break;
    case CL_DEVICE_PARENT_DEVICE:                               getCap<CL_DEVICE_PARENT_DEVICE                               >(src, srcSize, retSize); break;
    case CL_DEVICE_PARTITION_AFFINITY_DOMAIN:                   getCap<CL_DEVICE_PARTITION_AFFINITY_DOMAIN                   >(src, srcSize, retSize); break;
    case CL_DEVICE_PARTITION_MAX_SUB_DEVICES:                   getCap<CL_DEVICE_PARTITION_MAX_SUB_DEVICES                   >(src, srcSize, retSize); break;
    case CL_DEVICE_PARTITION_PROPERTIES:                        getCap<CL_DEVICE_PARTITION_PROPERTIES                        >(src, srcSize, retSize); break;
    case CL_DEVICE_PARTITION_TYPE:                              getCap<CL_DEVICE_PARTITION_TYPE                              >(src, srcSize, retSize); break;
    case CL_DEVICE_PIPE_MAX_ACTIVE_RESERVATIONS:                getCap<CL_DEVICE_PIPE_MAX_ACTIVE_RESERVATIONS                >(src, srcSize, retSize); break;
    case CL_DEVICE_PIPE_MAX_PACKET_SIZE:                        getCap<CL_DEVICE_PIPE_MAX_PACKET_SIZE                        >(src, srcSize, retSize); break;
    case CL_DEVICE_PLATFORM:                                    getCap<CL_DEVICE_PLATFORM                                    >(src, srcSize, retSize); break;
    case CL_DEVICE_PREFERRED_GLOBAL_ATOMIC_ALIGNMENT:           getCap<CL_DEVICE_PREFERRED_GLOBAL_ATOMIC_ALIGNMENT           >(src, srcSize, retSize); break;
    case CL_DEVICE_PREFERRED_INTEROP_USER_SYNC:                 getCap<CL_DEVICE_PREFERRED_INTEROP_USER_SYNC                 >(src, srcSize, retSize); break;
    case CL_DEVICE_PREFERRED_LOCAL_ATOMIC_ALIGNMENT:            getCap<CL_DEVICE_PREFERRED_LOCAL_ATOMIC_ALIGNMENT            >(src, srcSize, retSize); break;
    case CL_DEVICE_PREFERRED_PLATFORM_ATOMIC_ALIGNMENT:         getCap<CL_DEVICE_PREFERRED_PLATFORM_ATOMIC_ALIGNMENT         >(src, srcSize, retSize); break;
    case CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR:                 getCap<CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR                 >(src, srcSize, retSize); break;
    case CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE:               getCap<CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE               >(src, srcSize, retSize); break;
    case CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT:                getCap<CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT                >(src, srcSize, retSize); break;
    case CL_DEVICE_PREFERRED_VECTOR_WIDTH_HALF:                 getCap<CL_DEVICE_PREFERRED_VECTOR_WIDTH_HALF                 >(src, srcSize, retSize); break;
    case CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT:                  getCap<CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT                  >(src, srcSize, retSize); break;
    case CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG:                 getCap<CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG                 >(src, srcSize, retSize); break;
    case CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT:                getCap<CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT                >(src, srcSize, retSize); break;
    case CL_DEVICE_PRINTF_BUFFER_SIZE:                          getCap<CL_DEVICE_PRINTF_BUFFER_SIZE                          >(src, srcSize, retSize); break;
    case CL_DEVICE_PROFILE:                                     getStr<CL_DEVICE_PROFILE                                     >(src, srcSize, retSize); break;
    case CL_DEVICE_PROFILING_TIMER_RESOLUTION:                  getCap<CL_DEVICE_PROFILING_TIMER_RESOLUTION                  >(src, srcSize, retSize); break;
    case CL_DEVICE_QUEUE_ON_DEVICE_MAX_SIZE:                    getCap<CL_DEVICE_QUEUE_ON_DEVICE_MAX_SIZE                    >(src, srcSize, retSize); break;
    case CL_DEVICE_QUEUE_ON_DEVICE_PREFERRED_SIZE:              getCap<CL_DEVICE_QUEUE_ON_DEVICE_PREFERRED_SIZE              >(src, srcSize, retSize); break;
    case CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES:                  getCap<CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES                  >(src, srcSize, retSize); break;
    case CL_DEVICE_QUEUE_ON_HOST_PROPERTIES:                    getCap<CL_DEVICE_QUEUE_ON_HOST_PROPERTIES                    >(src, srcSize, retSize); break;
    case CL_DEVICE_SHARED_SYSTEM_MEM_CAPABILITIES_INTEL:        getCap<CL_DEVICE_SHARED_SYSTEM_MEM_CAPABILITIES_INTEL        >(src, srcSize, retSize); break;
    case CL_DEVICE_SINGLE_DEVICE_SHARED_MEM_CAPABILITIES_INTEL: getCap<CL_DEVICE_SINGLE_DEVICE_SHARED_MEM_CAPABILITIES_INTEL >(src, srcSize, retSize); break;
    case CL_DEVICE_SINGLE_FP_CONFIG:                            getCap<CL_DEVICE_SINGLE_FP_CONFIG                            >(src, srcSize, retSize); break;
    case CL_DEVICE_SLICE_COUNT_INTEL:                           getCap<CL_DEVICE_SLICE_COUNT_INTEL                           >(src, srcSize, retSize); break;
    case CL_DEVICE_SPIR_VERSIONS:                               getStr<CL_DEVICE_SPIR_VERSIONS                               >(src, srcSize, retSize); break;
    case CL_DEVICE_SUB_GROUP_INDEPENDENT_FORWARD_PROGRESS:      getCap<CL_DEVICE_SUB_GROUP_INDEPENDENT_FORWARD_PROGRESS      >(src, srcSize, retSize); break;
    case CL_DEVICE_SUB_GROUP_SIZES_INTEL:                       getCap<CL_DEVICE_SUB_GROUP_SIZES_INTEL                       >(src, srcSize, retSize); break;
    case CL_DEVICE_SVM_CAPABILITIES:                            getCap<CL_DEVICE_SVM_CAPABILITIES                            >(src, srcSize, retSize); break;
    case CL_DEVICE_TYPE:                                        getCap<CL_DEVICE_TYPE                                        >(src, srcSize, retSize); break;
    case CL_DEVICE_VENDOR:                                      getStr<CL_DEVICE_VENDOR                                      >(src, srcSize, retSize); break;
    case CL_DEVICE_VENDOR_ID:                                   getCap<CL_DEVICE_VENDOR_ID                                   >(src, srcSize, retSize); break;
    case CL_DEVICE_VERSION:                                     getStr<CL_DEVICE_VERSION                                     >(src, srcSize, retSize); break;
    case CL_DRIVER_VERSION:                                     getStr<CL_DRIVER_VERSION                                     >(src, srcSize, retSize); break; // clang-format on
    case CL_DEVICE_NUM_SIMULTANEOUS_INTEROPS_INTEL:
        if (simultaneousInterops.size() > 1u) {
            srcSize = retSize = sizeof(cl_uint);
            param = 1u;
            src = &param;
        }
        break;
    case CL_DEVICE_SIMULTANEOUS_INTEROPS_INTEL:
        if (simultaneousInterops.size() > 1u) {
            srcSize = retSize = sizeof(cl_uint) * simultaneousInterops.size();
            src = &simultaneousInterops[0];
        }
        break;
    case CL_DEVICE_REFERENCE_COUNT: {
        cl_int ref = this->getReference();
        DEBUG_BREAK_IF(ref != 1);
        param = static_cast<cl_uint>(ref);
        src = &param;
        retSize = srcSize = sizeof(param);
        break;
    }
    default:
        if (device.getDeviceInfo().imageSupport && getDeviceInfoForImage(paramName, src, srcSize, retSize)) {
            break;
        }
        DeviceHelper::getExtraDeviceInfo(*this, paramName, param, src, srcSize, retSize);
    }

    retVal = ::getInfo(paramValue, paramValueSize, src, srcSize);

    if (paramValueSizeRet) {
        *paramValueSizeRet = retSize;
    }

    return retVal;
}

bool ClDevice::getDeviceInfoForImage(cl_device_info paramName,
                                     const void *&src,
                                     size_t &srcSize,
                                     size_t &retSize) {
    switch (paramName) {
    case CL_DEVICE_MAX_READ_IMAGE_ARGS:
        getCap<CL_DEVICE_MAX_READ_IMAGE_ARGS>(src, srcSize, retSize);
        break;
    case CL_DEVICE_MAX_READ_WRITE_IMAGE_ARGS:
        getCap<CL_DEVICE_MAX_READ_WRITE_IMAGE_ARGS>(src, srcSize, retSize);
        break;
    case CL_DEVICE_MAX_WRITE_IMAGE_ARGS:
        getCap<CL_DEVICE_MAX_WRITE_IMAGE_ARGS>(src, srcSize, retSize);
        break;
    case CL_DEVICE_IMAGE2D_MAX_HEIGHT:
        getCap<CL_DEVICE_IMAGE2D_MAX_HEIGHT>(src, srcSize, retSize);
        break;
    case CL_DEVICE_IMAGE2D_MAX_WIDTH:
        getCap<CL_DEVICE_IMAGE2D_MAX_WIDTH>(src, srcSize, retSize);
        break;
    case CL_DEVICE_IMAGE3D_MAX_DEPTH:
        getCap<CL_DEVICE_IMAGE3D_MAX_DEPTH>(src, srcSize, retSize);
        break;
    case CL_DEVICE_IMAGE3D_MAX_HEIGHT:
        getCap<CL_DEVICE_IMAGE3D_MAX_HEIGHT>(src, srcSize, retSize);
        break;
    case CL_DEVICE_IMAGE3D_MAX_WIDTH:
        getCap<CL_DEVICE_IMAGE3D_MAX_WIDTH>(src, srcSize, retSize);
        break;
    case CL_DEVICE_IMAGE_BASE_ADDRESS_ALIGNMENT:
        getCap<CL_DEVICE_IMAGE_BASE_ADDRESS_ALIGNMENT>(src, srcSize, retSize);
        break;
    case CL_DEVICE_IMAGE_MAX_ARRAY_SIZE:
        getCap<CL_DEVICE_IMAGE_MAX_ARRAY_SIZE>(src, srcSize, retSize);
        break;
    case CL_DEVICE_IMAGE_MAX_BUFFER_SIZE:
        getCap<CL_DEVICE_IMAGE_MAX_BUFFER_SIZE>(src, srcSize, retSize);
        break;
    case CL_DEVICE_IMAGE_PITCH_ALIGNMENT:
        getCap<CL_DEVICE_IMAGE_PITCH_ALIGNMENT>(src, srcSize, retSize);
        break;
    case CL_DEVICE_PLANAR_YUV_MAX_WIDTH_INTEL:
        if (getDeviceInfo().nv12Extension)
            getCap<CL_DEVICE_PLANAR_YUV_MAX_WIDTH_INTEL>(src, srcSize, retSize);
        break;
    case CL_DEVICE_PLANAR_YUV_MAX_HEIGHT_INTEL:
        if (getDeviceInfo().nv12Extension)
            getCap<CL_DEVICE_PLANAR_YUV_MAX_HEIGHT_INTEL>(src, srcSize, retSize);
        break;
    default:
        return false;
    }
    return true;
}

bool Device::getDeviceAndHostTimer(uint64_t *deviceTimestamp, uint64_t *hostTimestamp) const {
    TimeStampData queueTimeStamp;
    bool retVal = getOSTime()->getCpuGpuTime(&queueTimeStamp);
    if (retVal) {
        uint64_t resolution = (uint64_t)getOSTime()->getDynamicDeviceTimerResolution(getHardwareInfo());
        *deviceTimestamp = queueTimeStamp.GPUTimeStamp * resolution;
    }

    retVal = getOSTime()->getCpuTime(hostTimestamp);
    return retVal;
}

bool Device::getHostTimer(uint64_t *hostTimestamp) const {
    return getOSTime()->getCpuTime(hostTimestamp);
}

} // namespace NEO
