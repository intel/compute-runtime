/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/cl_device/cl_device_info.h"

#include "shared/source/device/device.h"
#include "shared/source/device/device_info.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/os_time.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/cl_device/cl_device_get_cap.inl"
#include "opencl/source/cl_device/cl_device_info_map.h"
#include "opencl/source/cl_device/cl_device_vector.h"
#include "opencl/source/helpers/cl_gfx_core_helper.h"
#include "opencl/source/helpers/get_info_status_mapper.h"
#include "opencl/source/platform/platform.h"

namespace NEO {

using ClDeviceInfoTable::Map;

template <cl_device_info param>
inline void ClDevice::getStr(const void *&src,
                             size_t &size,
                             size_t &retSize) {
    src = Map<param>::getValue(*this);
    retSize = size = strlen(Map<param>::getValue(*this)) + 1;
}

template <>
inline void ClDevice::getCap<CL_DEVICE_MAX_WORK_ITEM_SIZES>(const void *&src,
                                                            size_t &size,
                                                            size_t &retSize) {
    src = getSharedDeviceInfo().maxWorkItemSizes;
    retSize = size = sizeof(getSharedDeviceInfo().maxWorkItemSizes);
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
    src = getSharedDeviceInfo().maxSubGroups.begin();
    retSize = size = (getSharedDeviceInfo().maxSubGroups.size() * sizeof(size_t));
}

cl_int ClDevice::getDeviceInfo(cl_device_info paramName,
                               size_t paramValueSize,
                               void *paramValue,
                               size_t *paramValueSizeRet) {
    cl_int retVal = CL_INVALID_VALUE;
    size_t srcSize = GetInfo::invalidSourceSize;
    size_t retSize = 0;
    size_t value = 0u;
    ClDeviceInfoParam param{};
    const void *src = nullptr;
    std::array<uint8_t, CL_UUID_SIZE_KHR> uuid;
    std::array<uint8_t, CL_LUID_SIZE_KHR> luid;

    // clang-format off
    // please keep alphabetical order
    switch (paramName) {
    case CL_DEVICE_ADDRESS_BITS:                                getCap<CL_DEVICE_ADDRESS_BITS                                >(src, srcSize, retSize); break;
    case CL_DEVICE_ATOMIC_FENCE_CAPABILITIES:                   getCap<CL_DEVICE_ATOMIC_FENCE_CAPABILITIES                   >(src, srcSize, retSize); break;
    case CL_DEVICE_ATOMIC_MEMORY_CAPABILITIES:                  getCap<CL_DEVICE_ATOMIC_MEMORY_CAPABILITIES                  >(src, srcSize, retSize); break;
    case CL_DEVICE_AVAILABLE:                                   getCap<CL_DEVICE_AVAILABLE                                   >(src, srcSize, retSize); break;
    case CL_DEVICE_AVC_ME_SUPPORTS_PREEMPTION_INTEL:            getCap<CL_DEVICE_AVC_ME_SUPPORTS_PREEMPTION_INTEL            >(src, srcSize, retSize); break;
    case CL_DEVICE_AVC_ME_SUPPORTS_TEXTURE_SAMPLER_USE_INTEL:   getCap<CL_DEVICE_AVC_ME_SUPPORTS_TEXTURE_SAMPLER_USE_INTEL   >(src, srcSize, retSize); break;
    case CL_DEVICE_AVC_ME_VERSION_INTEL:                        getCap<CL_DEVICE_AVC_ME_VERSION_INTEL                        >(src, srcSize, retSize); break;
    case CL_DEVICE_BUILT_IN_KERNELS:                            getStr<CL_DEVICE_BUILT_IN_KERNELS                            >(src, srcSize, retSize); break;
    case CL_DEVICE_COMPILER_AVAILABLE:                          getCap<CL_DEVICE_COMPILER_AVAILABLE                          >(src, srcSize, retSize); break;
    case CL_DEVICE_CROSS_DEVICE_SHARED_MEM_CAPABILITIES_INTEL:  getCap<CL_DEVICE_CROSS_DEVICE_SHARED_MEM_CAPABILITIES_INTEL  >(src, srcSize, retSize); break;
    case CL_DEVICE_DEVICE_MEM_CAPABILITIES_INTEL:               getCap<CL_DEVICE_DEVICE_MEM_CAPABILITIES_INTEL               >(src, srcSize, retSize); break;
    case CL_DEVICE_DOUBLE_FP_ATOMIC_CAPABILITIES_EXT:           getCap<CL_DEVICE_DOUBLE_FP_ATOMIC_CAPABILITIES_EXT           >(src, srcSize, retSize); break;
    case CL_DEVICE_DOUBLE_FP_CONFIG:                            getCap<CL_DEVICE_DOUBLE_FP_CONFIG                            >(src, srcSize, retSize); break;
    case CL_DEVICE_DRIVER_VERSION_INTEL:                        getCap<CL_DEVICE_DRIVER_VERSION_INTEL                        >(src, srcSize, retSize); break;
    case CL_DEVICE_ENDIAN_LITTLE:                               getCap<CL_DEVICE_ENDIAN_LITTLE                               >(src, srcSize, retSize); break;
    case CL_DEVICE_ERROR_CORRECTION_SUPPORT:                    getCap<CL_DEVICE_ERROR_CORRECTION_SUPPORT                    >(src, srcSize, retSize); break;
    case CL_DEVICE_EXECUTION_CAPABILITIES:                      getCap<CL_DEVICE_EXECUTION_CAPABILITIES                      >(src, srcSize, retSize); break;
    case CL_DEVICE_EXTENSIONS:                                  getStr<CL_DEVICE_EXTENSIONS                                  >(src, srcSize, retSize); break;
    case CL_DEVICE_EXTERNAL_MEMORY_IMPORT_HANDLE_TYPES_KHR:     getCap<CL_DEVICE_EXTERNAL_MEMORY_IMPORT_HANDLE_TYPES_KHR     >(src, srcSize, retSize); break;
    case CL_DEVICE_GENERIC_ADDRESS_SPACE_SUPPORT:               getCap<CL_DEVICE_GENERIC_ADDRESS_SPACE_SUPPORT               >(src, srcSize, retSize); break;
    case CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE:                   getCap<CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE                   >(src, srcSize, retSize); break;
    case CL_DEVICE_GLOBAL_MEM_CACHE_SIZE:                       getCap<CL_DEVICE_GLOBAL_MEM_CACHE_SIZE                       >(src, srcSize, retSize); break;
    case CL_DEVICE_GLOBAL_MEM_CACHE_TYPE:                       getCap<CL_DEVICE_GLOBAL_MEM_CACHE_TYPE                       >(src, srcSize, retSize); break;
    case CL_DEVICE_GLOBAL_MEM_SIZE:                             getCap<CL_DEVICE_GLOBAL_MEM_SIZE                             >(src, srcSize, retSize); break;
    case CL_DEVICE_GLOBAL_VARIABLE_PREFERRED_TOTAL_SIZE:        getCap<CL_DEVICE_GLOBAL_VARIABLE_PREFERRED_TOTAL_SIZE        >(src, srcSize, retSize); break;
    case CL_DEVICE_HALF_FP_ATOMIC_CAPABILITIES_EXT:             getCap<CL_DEVICE_HALF_FP_ATOMIC_CAPABILITIES_EXT             >(src, srcSize, retSize); break;
    case CL_DEVICE_HALF_FP_CONFIG:                              getCap<CL_DEVICE_HALF_FP_CONFIG                              >(src, srcSize, retSize); break;
    case CL_DEVICE_HOST_MEM_CAPABILITIES_INTEL:                 getCap<CL_DEVICE_HOST_MEM_CAPABILITIES_INTEL                 >(src, srcSize, retSize); break;
    case CL_DEVICE_HOST_UNIFIED_MEMORY:                         getCap<CL_DEVICE_HOST_UNIFIED_MEMORY                         >(src, srcSize, retSize); break;
    case CL_DEVICE_IL_VERSION:                                  getStr<CL_DEVICE_IL_VERSION                                  >(src, srcSize, retSize); break;
    case CL_DEVICE_IMAGE_SUPPORT:                               getCap<CL_DEVICE_IMAGE_SUPPORT                               >(src, srcSize, retSize); break;
    case CL_DEVICE_LATEST_CONFORMANCE_VERSION_PASSED:           getStr<CL_DEVICE_LATEST_CONFORMANCE_VERSION_PASSED           >(src, srcSize, retSize); break;
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
    case CL_DEVICE_NON_UNIFORM_WORK_GROUP_SUPPORT:              getCap<CL_DEVICE_NON_UNIFORM_WORK_GROUP_SUPPORT              >(src, srcSize, retSize); break;
    case CL_DEVICE_NUMERIC_VERSION:                             getCap<CL_DEVICE_NUMERIC_VERSION                             >(src, srcSize, retSize); break;
    case CL_DEVICE_OPENCL_C_VERSION:                            getStr<CL_DEVICE_OPENCL_C_VERSION                            >(src, srcSize, retSize); break;
    case CL_DEVICE_PARENT_DEVICE:                               getCap<CL_DEVICE_PARENT_DEVICE                               >(src, srcSize, retSize); break;
    case CL_DEVICE_PARTITION_AFFINITY_DOMAIN:                   getCap<CL_DEVICE_PARTITION_AFFINITY_DOMAIN                   >(src, srcSize, retSize); break;
    case CL_DEVICE_PARTITION_MAX_SUB_DEVICES:                   getCap<CL_DEVICE_PARTITION_MAX_SUB_DEVICES                   >(src, srcSize, retSize); break;
    case CL_DEVICE_PIPE_MAX_ACTIVE_RESERVATIONS:                getCap<CL_DEVICE_PIPE_MAX_ACTIVE_RESERVATIONS                >(src, srcSize, retSize); break;
    case CL_DEVICE_PIPE_MAX_PACKET_SIZE:                        getCap<CL_DEVICE_PIPE_MAX_PACKET_SIZE                        >(src, srcSize, retSize); break;
    case CL_DEVICE_PIPE_SUPPORT:                                getCap<CL_DEVICE_PIPE_SUPPORT                                >(src, srcSize, retSize); break;
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
    case CL_DEVICE_PREFERRED_WORK_GROUP_SIZE_MULTIPLE:          getCap<CL_DEVICE_PREFERRED_WORK_GROUP_SIZE_MULTIPLE          >(src, srcSize, retSize); break;
    case CL_DEVICE_PRINTF_BUFFER_SIZE:                          getCap<CL_DEVICE_PRINTF_BUFFER_SIZE                          >(src, srcSize, retSize); break;
    case CL_DEVICE_PROFILE:                                     getStr<CL_DEVICE_PROFILE                                     >(src, srcSize, retSize); break;
    case CL_DEVICE_PROFILING_TIMER_RESOLUTION:                  getCap<CL_DEVICE_PROFILING_TIMER_RESOLUTION                  >(src, srcSize, retSize); break;
    case CL_DEVICE_QUEUE_ON_DEVICE_MAX_SIZE:                    getCap<CL_DEVICE_QUEUE_ON_DEVICE_MAX_SIZE                    >(src, srcSize, retSize); break;
    case CL_DEVICE_QUEUE_ON_DEVICE_PREFERRED_SIZE:              getCap<CL_DEVICE_QUEUE_ON_DEVICE_PREFERRED_SIZE              >(src, srcSize, retSize); break;
    case CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES:                  getCap<CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES                  >(src, srcSize, retSize); break;
    case CL_DEVICE_QUEUE_ON_HOST_PROPERTIES:                    getCap<CL_DEVICE_QUEUE_ON_HOST_PROPERTIES                    >(src, srcSize, retSize); break;
    case CL_DEVICE_SHARED_SYSTEM_MEM_CAPABILITIES_INTEL:        getCap<CL_DEVICE_SHARED_SYSTEM_MEM_CAPABILITIES_INTEL        >(src, srcSize, retSize); break;
    case CL_DEVICE_SINGLE_DEVICE_SHARED_MEM_CAPABILITIES_INTEL: getCap<CL_DEVICE_SINGLE_DEVICE_SHARED_MEM_CAPABILITIES_INTEL >(src, srcSize, retSize); break;
    case CL_DEVICE_SINGLE_FP_ATOMIC_CAPABILITIES_EXT:           getCap<CL_DEVICE_SINGLE_FP_ATOMIC_CAPABILITIES_EXT           >(src, srcSize, retSize); break;
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
    case CL_DEVICE_WORK_GROUP_COLLECTIVE_FUNCTIONS_SUPPORT:     getCap<CL_DEVICE_WORK_GROUP_COLLECTIVE_FUNCTIONS_SUPPORT     >(src, srcSize, retSize); break;
    case CL_DEVICE_INTEGER_DOT_PRODUCT_CAPABILITIES_KHR:                                 getCap<CL_DEVICE_INTEGER_DOT_PRODUCT_CAPABILITIES_KHR                                 >(src, srcSize, retSize); break;
    case CL_DEVICE_INTEGER_DOT_PRODUCT_ACCELERATION_PROPERTIES_4x8BIT_PACKED_KHR:        getCap<CL_DEVICE_INTEGER_DOT_PRODUCT_ACCELERATION_PROPERTIES_4x8BIT_PACKED_KHR        >(src, srcSize, retSize); break;
    case CL_DEVICE_INTEGER_DOT_PRODUCT_ACCELERATION_PROPERTIES_8BIT_KHR:                 getCap<CL_DEVICE_INTEGER_DOT_PRODUCT_ACCELERATION_PROPERTIES_8BIT_KHR                 >(src, srcSize, retSize); break;
    case CL_DRIVER_VERSION:                                     getStr<CL_DRIVER_VERSION                                     >(src, srcSize, retSize); break; // clang-format on
    case CL_DEVICE_DEVICE_ENQUEUE_CAPABILITIES:
        if (paramValueSize == sizeof(cl_bool)) {
            srcSize = retSize = sizeof(cl_bool);
            param.boolean = CL_FALSE;
            src = &param.boolean;
        } else {
            getCap<CL_DEVICE_DEVICE_ENQUEUE_CAPABILITIES>(src, srcSize, retSize);
        }
        break;
    case CL_DEVICE_NUM_SIMULTANEOUS_INTEROPS_INTEL:
        if (simultaneousInterops.size() > 1u) {
            srcSize = retSize = sizeof(cl_uint);
            param.uint = 1u;
            src = &param.uint;
        }
        break;
    case CL_DEVICE_SIMULTANEOUS_INTEROPS_INTEL:
        if (simultaneousInterops.size() > 1u) {
            srcSize = retSize = sizeof(cl_uint) * simultaneousInterops.size();
            src = &simultaneousInterops[0];
        }
        break;
    case CL_DEVICE_QUEUE_FAMILY_PROPERTIES_INTEL:
        src = deviceInfo.queueFamilyProperties.data();
        retSize = srcSize = deviceInfo.queueFamilyProperties.size() * sizeof(cl_queue_family_properties_intel);
        break;
    case CL_DEVICE_REFERENCE_COUNT: {
        cl_int ref = this->getReference();
        DEBUG_BREAK_IF(ref != 1 && !deviceInfo.parentDevice);
        param.uint = static_cast<cl_uint>(ref);
        src = &param.uint;
        retSize = srcSize = sizeof(param.uint);
        break;
    }
    case CL_DEVICE_PARTITION_PROPERTIES:
        getCap<CL_DEVICE_PARTITION_PROPERTIES>(src, srcSize, retSize);
        if (deviceInfo.partitionProperties[0] == 0) {
            retSize = srcSize = sizeof(deviceInfo.partitionProperties[0]);
        }
        break;
    case CL_DEVICE_PARTITION_TYPE:
        getCap<CL_DEVICE_PARTITION_TYPE>(src, srcSize, retSize);
        if (deviceInfo.partitionType[0] == 0) {
            retSize = srcSize = sizeof(deviceInfo.partitionType[0]);
        }
        break;
    case CL_DEVICE_OPENCL_C_FEATURES:
        src = deviceInfo.openclCFeatures.data();
        retSize = srcSize = deviceInfo.openclCFeatures.size() * sizeof(cl_name_version);
        break;
    case CL_DEVICE_ILS_WITH_VERSION:
        src = deviceInfo.ilsWithVersion.data();
        retSize = srcSize = deviceInfo.ilsWithVersion.size() * sizeof(cl_name_version);
        break;
    case CL_DEVICE_BUILT_IN_KERNELS_WITH_VERSION:
        src = deviceInfo.builtInKernelsWithVersion.data();
        retSize = srcSize = deviceInfo.builtInKernelsWithVersion.size() * sizeof(cl_name_version);
        break;
    case CL_DEVICE_OPENCL_C_ALL_VERSIONS:
        src = deviceInfo.openclCAllVersions.data();
        retSize = srcSize = deviceInfo.openclCAllVersions.size() * sizeof(cl_name_version);
        break;
    case CL_DEVICE_EXTENSIONS_WITH_VERSION:
        std::call_once(initializeExtensionsWithVersionOnce, [this]() {
            this->initializeExtensionsWithVersion();
        });
        src = deviceInfo.extensionsWithVersion.data();
        retSize = srcSize = deviceInfo.extensionsWithVersion.size() * sizeof(cl_name_version);
        break;
    case CL_DEVICE_SUPPORTED_THREAD_ARBITRATION_POLICY_INTEL:
        src = deviceInfo.supportedThreadArbitrationPolicies.data();
        retSize = srcSize = deviceInfo.supportedThreadArbitrationPolicies.size() * sizeof(cl_uint);
        break;
    case CL_DEVICE_IP_VERSION_INTEL: {
        uint32_t hwIpVersion = getHardwareInfo().ipVersionOverrideExposedToTheApplication.value;
        if (0 == hwIpVersion) {
            auto &compilerProductHelper = device.getCompilerProductHelper();
            hwIpVersion = compilerProductHelper.getHwIpVersion(getHardwareInfo());
        }
        param.uint = static_cast<cl_version>(hwIpVersion);
        src = &param.uint;
        retSize = srcSize = sizeof(cl_version);
        break;
    }
    case CL_DEVICE_ID_INTEL:
        param.uint = getHardwareInfo().platform.usDeviceID;
        src = &param.uint;
        retSize = srcSize = sizeof(cl_uint);
        break;
    case CL_DEVICE_NUM_SLICES_INTEL:
        param.uint = static_cast<cl_uint>(getHardwareInfo().gtSystemInfo.SliceCount * std::max(device.getNumGenericSubDevices(), 1u));
        src = &param.uint;
        retSize = srcSize = sizeof(cl_uint);
        break;
    case CL_DEVICE_NUM_SUB_SLICES_PER_SLICE_INTEL: {
        param.uint = static_cast<cl_uint>(getNumSubSlicesPerSlice(getHardwareInfo()));
        src = &param.uint;
        retSize = srcSize = sizeof(cl_uint);
        break;
    }
    case CL_DEVICE_NUM_EUS_PER_SUB_SLICE_INTEL:
        param.uint = getHardwareInfo().gtSystemInfo.MaxEuPerSubSlice;
        src = &param.uint;
        retSize = srcSize = sizeof(cl_uint);
        break;
    case CL_DEVICE_NUM_THREADS_PER_EU_INTEL: {
        const auto &gtSysInfo = getHardwareInfo().gtSystemInfo;
        param.uint = gtSysInfo.ThreadCount / gtSysInfo.EUCount;
        src = &param.uint;
        retSize = srcSize = sizeof(cl_uint);
        break;
    }
    case CL_DEVICE_FEATURE_CAPABILITIES_INTEL: {
        auto &clGfxCoreHelper = this->getRootDeviceEnvironment().getHelper<ClGfxCoreHelper>();
        param.bitfield = clGfxCoreHelper.getSupportedDeviceFeatureCapabilities(this->getRootDeviceEnvironment());
        src = &param.bitfield;
        retSize = srcSize = sizeof(cl_device_feature_capabilities_intel);
        break;
    }
    case CL_DEVICE_PCI_BUS_INFO_KHR:
        if (isPciBusInfoValid()) {
            src = &deviceInfo.pciBusInfo;
            retSize = srcSize = sizeof(deviceInfo.pciBusInfo);
        }
        break;
    case CL_DEVICE_UUID_KHR:
        if (device.getUuid(uuid) == false) {
            device.generateUuid(uuid);
        }
        src = uuid.data();
        retSize = srcSize = CL_UUID_SIZE_KHR;
        break;
    case CL_DRIVER_UUID_KHR:
        const void *tmpUuid;
        getStr<CL_DRIVER_UUID_KHR>(tmpUuid, srcSize, retSize);
        uuid.fill(0);
        memcpy_s(uuid.data(), srcSize, tmpUuid, srcSize);
        src = uuid.data();
        retSize = srcSize = CL_UUID_SIZE_KHR;
        break;
    case CL_DEVICE_LUID_VALID_KHR:
        param.boolean = device.verifyAdapterLuid();
        src = &param.boolean;
        retSize = srcSize = sizeof(cl_bool);
        break;
    case CL_DEVICE_LUID_KHR:
        if (paramValue != nullptr) {
            memcpy_s(luid.data(), CL_LUID_SIZE_KHR, paramValue, CL_LUID_SIZE_KHR);
        } else {
            luid = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
        }
        device.getAdapterLuid(luid);
        src = luid.data();
        retSize = srcSize = CL_LUID_SIZE_KHR;
        break;
    case CL_DEVICE_NODE_MASK_KHR:
        if (paramValue != nullptr) {
            memcpy_s(&param.uint, sizeof(cl_uint), paramValue, sizeof(cl_uint));
        } else {
            param.uint = 0;
        }
        device.getAdapterMask(param.uint);
        src = &param.uint;
        retSize = srcSize = sizeof(cl_uint);
        break;
    case CL_DEVICE_EU_THREAD_COUNTS_INTEL:
        src = getSharedDeviceInfo().threadsPerEUConfigs.begin();
        retSize = srcSize = (getSharedDeviceInfo().threadsPerEUConfigs.size() * sizeof(uint32_t));
        break;
    default:
        if (getDeviceInfoForImage(paramName, src, srcSize, retSize)) {
            if (false == getSharedDeviceInfo().imageSupport) {
                src = &value;
            }
        } else if (getDeviceInfoExtra(paramName, param, src, srcSize, retSize)) {
            break;
        }
        break;
    }

    auto getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, src, srcSize);

    retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
    GetInfo::setParamValueReturnSize(paramValueSizeRet, retSize, getInfoStatus);

    return retVal;
}

bool ClDevice::getDeviceInfoForImage(cl_device_info paramName,
                                     const void *&src,
                                     size_t &srcSize,
                                     size_t &retSize) {
    bool retVal = true;
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
        if (deviceInfo.nv12Extension) {
            getCap<CL_DEVICE_PLANAR_YUV_MAX_WIDTH_INTEL>(src, srcSize, retSize);
            break;
        }
        retVal = false;
        break;
    case CL_DEVICE_PLANAR_YUV_MAX_HEIGHT_INTEL:
        if (deviceInfo.nv12Extension) {
            getCap<CL_DEVICE_PLANAR_YUV_MAX_HEIGHT_INTEL>(src, srcSize, retSize);
            break;
        }
        retVal = false;
        break;
    default:
        retVal = false;
    }
    return retVal;
}

} // namespace NEO
