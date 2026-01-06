/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "CL/cl.h"
#include "CL/cl_ext.h"

/**********************************
 * Internal only queue properties *
 **********************************/
// Intel evaluation now. Remove it after approval for public release
#define CL_DEVICE_DRIVER_VERSION_INTEL 0x10010

#define CL_DEVICE_DRIVER_VERSION_INTEL_NEO1 0x454E4831 // Driver version is ENH1

/*********************************************
 * Internal only kernel exec info properties *
 *********************************************/

#define CL_KERNEL_EXEC_INFO_KERNEL_TYPE_INTEL 0x1000C
#define CL_KERNEL_EXEC_INFO_DEFAULT_TYPE_INTEL 0x1000D
#define CL_KERNEL_EXEC_INFO_CONCURRENT_TYPE_INTEL 0x1000E

/*********************************
 * cl_intel_debug_info extension *
 *********************************/
#define cl_intel_debug_info 1

// New queries for clGetProgramInfo:
#define CL_PROGRAM_DEBUG_INFO_INTEL 0x4100
#define CL_PROGRAM_DEBUG_INFO_SIZES_INTEL 0x4101

// New queries for clGetKernelInfo:
#define CL_KERNEL_BINARY_PROGRAM_INTEL 0x407D
#define CL_KERNEL_BINARIES_INTEL 0x4102
#define CL_KERNEL_BINARY_SIZES_INTEL 0x4103
#define CL_KERNEL_BINARY_GPU_ADDRESS_INTEL 0x10010

/********************************************
 * event properties for performance counter *
 ********************************************/
/* performance counter */
#define CL_PROFILING_COMMAND_PERFCOUNTERS_INTEL 0x407F

/**************************
 * Internal only cl types *
 **************************/

using cl_execution_info_kernel_type_intel = cl_uint;
using cl_mem_flags_intel = cl_mem_flags;
using cl_unified_shared_memory_capabilities_intel = cl_bitfield;

#if !defined(cl_intel_unified_shared_memory)
using cl_mem_alloc_flags_intel = cl_bitfield;
using cl_mem_properties_intel = cl_bitfield;
using cl_mem_info_intel = cl_uint;
using cl_mem_advice_intel = cl_uint;
using cl_unified_shared_memory_type_intel = cl_uint;
#endif

/******************************
 * Internal only cl_mem_flags *
 ******************************/

#define CL_MEM_FLAGS_INTEL 0x10001
#define CL_MEM_LOCALLY_UNCACHED_RESOURCE (1 << 18)
#define CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE (1 << 25)
#define CL_MEM_48BIT_RESOURCE_INTEL (1 << 26)

#define CL_MEM_DEVICE_ID_INTEL_DEPRECATED 0x10011

// Used with clEnqueueVerifyMemory
#define CL_MEM_COMPARE_EQUAL 0u
#define CL_MEM_COMPARE_NOT_EQUAL 1u

#define CL_MEM_FORCE_LINEAR_STORAGE_INTEL (1 << 19)
#define CL_MEM_FORCE_HOST_MEMORY_INTEL (1 << 20)

#define CL_MEM_ALLOCATION_HANDLE_INTEL 0x10050
#define CL_MEM_USES_COMPRESSION_INTEL 0x10051

// Used with createBuffer
#define CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL (1 << 23)

/******************************
 *        UNIFIED MEMORY       *
 *******************************/

#if !defined(cl_intel_unified_shared_memory)

/* cl_device_info */
#define CL_DEVICE_HOST_MEM_CAPABILITIES_INTEL 0x4190
#define CL_DEVICE_DEVICE_MEM_CAPABILITIES_INTEL 0x4191
#define CL_DEVICE_SINGLE_DEVICE_SHARED_MEM_CAPABILITIES_INTEL 0x4192
#define CL_DEVICE_CROSS_DEVICE_SHARED_MEM_CAPABILITIES_INTEL 0x4193
#define CL_DEVICE_SHARED_SYSTEM_MEM_CAPABILITIES_INTEL 0x4194

/* cl_unified_shared_memory_capabilities_intel - bitfield */
#define CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL (1 << 0)
#define CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL (1 << 1)
#define CL_UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS_INTEL (1 << 2)
#define CL_UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS_INTEL (1 << 3)

/* cl_mem_properties_intel */
#define CL_MEM_ALLOC_FLAGS_INTEL 0x4195
#define CL_MEM_ALLOC_USE_HOST_PTR_INTEL 0x1000F

/* cl_mem_alloc_flags_intel - bitfield */
#define CL_MEM_ALLOC_WRITE_COMBINED_INTEL (1 << 0)
#define CL_MEM_ALLOC_INITIAL_PLACEMENT_DEVICE_INTEL (1 << 1)
#define CL_MEM_ALLOC_INITIAL_PLACEMENT_HOST_INTEL (1 << 2)

/* cl_mem_alloc_info_intel */
#define CL_MEM_ALLOC_TYPE_INTEL 0x419A
#define CL_MEM_ALLOC_BASE_PTR_INTEL 0x419B
#define CL_MEM_ALLOC_SIZE_INTEL 0x419C
#define CL_MEM_ALLOC_DEVICE_INTEL 0x419D

/* cl_unified_shared_memory_type_intel */
#define CL_MEM_TYPE_UNKNOWN_INTEL 0x4196
#define CL_MEM_TYPE_HOST_INTEL 0x4197
#define CL_MEM_TYPE_DEVICE_INTEL 0x4198
#define CL_MEM_TYPE_SHARED_INTEL 0x4199

/* cl_command_type */
#define CL_COMMAND_MEMFILL_INTEL 0x4204
#define CL_COMMAND_MEMCPY_INTEL 0x4205
#define CL_COMMAND_MIGRATEMEM_INTEL 0x4206
#define CL_COMMAND_MEMADVISE_INTEL 0x4207

#endif

/* cl_mem_alloc_flags_intel - bitfield */
#define CL_MEM_ALLOC_DEFAULT_INTEL 0

/* cl_mem_properties_intel */
#define CL_MEM_ALLOC_USE_HOST_PTR_INTEL 0x1000F

/* cl_command_type */
#define CL_COMMAND_MEMSET_INTEL 0x4204

/******************************
 *  THREAD ARBITRATION POLICY  *
 *******************************/

/* cl_device_info */
#define CL_DEVICE_SUPPORTED_THREAD_ARBITRATION_POLICY_INTEL 0x4208

/* cl_kernel_exec_info */
#if !defined(cl_intel_unified_shared_memory)
#define CL_KERNEL_EXEC_INFO_INDIRECT_HOST_ACCESS_INTEL 0x4200
#define CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL 0x4201
#define CL_KERNEL_EXEC_INFO_INDIRECT_SHARED_ACCESS_INTEL 0x4202
#define CL_KERNEL_EXEC_INFO_USM_PTRS_INTEL 0x4203
#endif

#define CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_OLDEST_FIRST_INTEL 0x10022
#define CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_ROUND_ROBIN_INTEL 0x10023
#define CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_AFTER_DEPENDENCY_ROUND_ROBIN_INTEL 0x10024
#define CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_INTEL 0x10025
#define CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_STALL_BASED_ROUND_ROBIN_INTEL 0x10026

/******************************
 *    SLICE COUNT SELECTING    *
 *******************************/

/* cl_device_info */
#define CL_DEVICE_SLICE_COUNT_INTEL 0x10020

/* cl_queue_properties */
#define CL_QUEUE_SLICE_COUNT_INTEL 0x10021

/******************************
 *   QUEUE FAMILY SELECTING    *
 *******************************/

#if !defined(cl_intel_command_queue_families)

/* cl_device_info */
#define CL_DEVICE_QUEUE_FAMILY_PROPERTIES_INTEL 0x418B

/* cl_queue_properties */
#define CL_QUEUE_FAMILY_INTEL 0x418C
#define CL_QUEUE_INDEX_INTEL 0x418D

/* cl_command_queue_capabilities_intel */
#define CL_QUEUE_DEFAULT_CAPABILITIES_INTEL 0
#define CL_QUEUE_CAPABILITY_CREATE_SINGLE_QUEUE_EVENTS_INTEL (1 << 0)
#define CL_QUEUE_CAPABILITY_CREATE_CROSS_QUEUE_EVENTS_INTEL (1 << 1)
#define CL_QUEUE_CAPABILITY_SINGLE_QUEUE_EVENT_WAIT_LIST_INTEL (1 << 2)
#define CL_QUEUE_CAPABILITY_CROSS_QUEUE_EVENT_WAIT_LIST_INTEL (1 << 3)
#define CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL (1 << 8)
#define CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_RECT_INTEL (1 << 9)
#define CL_QUEUE_CAPABILITY_MAP_BUFFER_INTEL (1 << 10)
#define CL_QUEUE_CAPABILITY_FILL_BUFFER_INTEL (1 << 11)
#define CL_QUEUE_CAPABILITY_TRANSFER_IMAGE_INTEL (1 << 12)
#define CL_QUEUE_CAPABILITY_MAP_IMAGE_INTEL (1 << 13)
#define CL_QUEUE_CAPABILITY_FILL_IMAGE_INTEL (1 << 14)
#define CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_IMAGE_INTEL (1 << 15)
#define CL_QUEUE_CAPABILITY_TRANSFER_IMAGE_BUFFER_INTEL (1 << 16)
#define CL_QUEUE_CAPABILITY_MARKER_INTEL (1 << 24)
#define CL_QUEUE_CAPABILITY_BARRIER_INTEL (1 << 25)
#define CL_QUEUE_CAPABILITY_KERNEL_INTEL (1 << 26)

typedef cl_bitfield cl_command_queue_capabilities_intel;

#define CL_QUEUE_FAMILY_MAX_NAME_SIZE_INTEL 64
typedef struct _cl_queue_family_properties_intel {
    cl_command_queue_properties properties;
    cl_command_queue_capabilities_intel capabilities;
    cl_uint count;
    char name[CL_QUEUE_FAMILY_MAX_NAME_SIZE_INTEL];
} cl_queue_family_properties_intel;

#endif

/******************************
 *   DEVICE ATTRIBUTE QUERY    *
 *******************************/

#if !defined(cl_intel_device_attribute_query)

/* For GPU devices, version 1.0.0: */
#define CL_DEVICE_IP_VERSION_INTEL 0x4250
#define CL_DEVICE_ID_INTEL 0x4251
#define CL_DEVICE_NUM_SLICES_INTEL 0x4252
#define CL_DEVICE_NUM_SUB_SLICES_PER_SLICE_INTEL 0x4253
#define CL_DEVICE_NUM_EUS_PER_SUB_SLICE_INTEL 0x4254
#define CL_DEVICE_NUM_THREADS_PER_EU_INTEL 0x4255
#define CL_DEVICE_FEATURE_CAPABILITIES_INTEL 0x4256

typedef cl_bitfield cl_device_feature_capabilities_intel;

/* For GPU devices, version 1.0.0: */
#define CL_DEVICE_FEATURE_FLAG_DP4A_INTEL (1 << 0)
#define CL_DEVICE_FEATURE_FLAG_DPAS_INTEL (1 << 1)

#endif

////// RESOURCE BARRIER EXT
#define CL_COMMAND_RESOURCE_BARRIER 0x10010

typedef cl_uint cl_resource_barrier_type;
#define CL_RESOURCE_BARRIER_TYPE_ACQUIRE 0x1 // FLUSH+EVICT
#define CL_RESOURCE_BARRIER_TYPE_RELEASE 0x2 // FLUSH
#define CL_RESOURCE_BARRIER_TYPE_DISCARD 0x3 // DISCARD

typedef cl_uint cl_resource_memory_scope;
#define CL_MEMORY_SCOPE_DEVICE 0x0          // INCLUDES CROSS-TILE
#define CL_MEMORY_SCOPE_ALL_SVM_DEVICES 0x1 // CL_MEMORY_SCOPE_DEVICE + CROSS-DEVICE

#pragma pack(push, 1)
typedef struct _cl_resource_barrier_descriptor_intel {
    void *svmAllocationPointer;
    cl_mem memObject;
    cl_resource_barrier_type type;
    cl_resource_memory_scope scope;
} cl_resource_barrier_descriptor_intel;
#pragma pack(pop)

/****************************************
 * cl_khr_pci_bus_info extension *
 ***************************************/

#if !defined(cl_khr_pci_bus_info)

#define cl_khr_pci_bus_info 1

// New queries for clGetDeviceInfo:
#define CL_DEVICE_PCI_BUS_INFO_KHR 0x410F

typedef struct _cl_device_pci_bus_info_khr {
    cl_uint pci_domain;
    cl_uint pci_bus;
    cl_uint pci_device;
    cl_uint pci_function;
} cl_device_pci_bus_info_khr;

#endif

/************************************************
 *   cl_intel_mem_compression_hints extension    *
 *************************************************/
#define CL_MEM_COMPRESSED_HINT_INTEL (1u << 21)
#define CL_MEM_UNCOMPRESSED_HINT_INTEL (1u << 22)

// New query for clGetDeviceInfo:
#define CL_MEM_COMPRESSED_INTEL 0x417D

/* cl_queue_properties */
#define CL_QUEUE_MDAPI_PROPERTIES_INTEL 0x425E
#define CL_QUEUE_MDAPI_CONFIGURATION_INTEL 0x425F

typedef cl_bitfield cl_command_queue_mdapi_properties_intel;

/* cl_command_queue_mdapi_properties_intel - bitfield */
#define CL_QUEUE_MDAPI_ENABLE_INTEL (1 << 0)

/************************************************
 *   cl_khr_external_memory extension    *
 *************************************************/

#if !defined(cl_khr_external_memory)

/* clGetPlatformInfo */
#define CL_PLATFORM_EXTERNAL_MEMORY_IMPORT_HANDLE_TYPES_KHR 0x2044

/* clGetDeviceInfo */
#define CL_DEVICE_EXTERNAL_MEMORY_IMPORT_HANDLE_TYPES_KHR 0x204F

/* clCreateBufferWithProperties and clCreateImageWithProperties */
#define CL_DEVICE_HANDLE_LIST_KHR 0x2051
#define CL_DEVICE_HANDLE_LIST_END_KHR 0

#endif

#if !defined(cl_khr_external_memory_opaque_fd)
#define CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_FD_KHR 0x2060
#endif

#if !defined(cl_khr_external_memory_win32)
#define CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR 0x2061
#define CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KMT_KHR 0x2062
#endif

#if !defined(cl_khr_external_memory_dx)
#define CL_EXTERNAL_MEMORY_HANDLE_D3D11_TEXTURE_KHR 0x2063
#define CL_EXTERNAL_MEMORY_HANDLE_D3D11_TEXTURE_KMT_KHR 0x2064
#define CL_EXTERNAL_MEMORY_HANDLE_D3D12_HEAP_KHR 0x2065
#define CL_EXTERNAL_MEMORY_HANDLE_D3D12_RESOURCE_KHR 0x2066
#endif

#if !defined(cl_khr_external_memory_dma_buf)
#define CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR 0x2067
#endif

// cl_intel_variable_eu_thread_count
#define CL_DEVICE_EU_THREAD_COUNTS_INTEL 0x1000A // placeholder
#define CL_KERNEL_EU_THREAD_COUNT_INTEL 0x1000B  // placeholder

#if !defined(cl_intel_maximum_registers)
#define CL_KERNEL_REGISTER_COUNT_INTEL 0x425B
#endif

/*************************************************
 *   cl_khr_spirv_queries extension              *
 *************************************************/

#ifndef cl_khr_spirv_queries
#define cl_khr_spirv_queries 1
#define CL_KHR_SPIRV_QUERIES_EXTENSION_NAME "cl_khr_spirv_queries"
#define CL_DEVICE_SPIRV_EXTENDED_INSTRUCTION_SETS_KHR 0x12B9
#define CL_DEVICE_SPIRV_EXTENSIONS_KHR 0x12BA
#define CL_DEVICE_SPIRV_CAPABILITIES_KHR 0x12BB
#endif

/*************************************************
 *   cl_ext_float_atomics extension              *
 *************************************************/

#if !defined(cl_ext_float_atomics)

#define cl_ext_float_atomics 1

#define CL_DEVICE_SINGLE_FP_ATOMIC_CAPABILITIES_EXT 0x4231
#define CL_DEVICE_DOUBLE_FP_ATOMIC_CAPABILITIES_EXT 0x4232
#define CL_DEVICE_HALF_FP_ATOMIC_CAPABILITIES_EXT 0x4233

typedef cl_bitfield cl_device_fp_atomic_capabilities_ext;

#define CL_DEVICE_GLOBAL_FP_ATOMIC_LOAD_STORE_EXT (1 << 0)
#define CL_DEVICE_GLOBAL_FP_ATOMIC_ADD_EXT (1 << 1)
#define CL_DEVICE_GLOBAL_FP_ATOMIC_MIN_MAX_EXT (1 << 2)

/* bits 3 - 15 are currently unused */

#define CL_DEVICE_LOCAL_FP_ATOMIC_LOAD_STORE_EXT (1 << 16)
#define CL_DEVICE_LOCAL_FP_ATOMIC_ADD_EXT (1 << 17)
#define CL_DEVICE_LOCAL_FP_ATOMIC_MIN_MAX_EXT (1 << 18)

/* bits 19 and beyond are currently unused */

#endif

/*************************************************
 *   CL_PLATFORM_UNLOADABLE_KHR                  *
 *************************************************/

#if !defined(CL_PLATFORM_UNLOADABLE_KHR)
#define CL_PLATFORM_UNLOADABLE_KHR 0x0921
#endif
