/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "CL/cl.h"

/**********************************
 * Internal only queue properties *
 **********************************/
// Intel evaluation now. Remove it after approval for public release
#define CL_DEVICE_DRIVER_VERSION_INTEL 0x10010

#define CL_DEVICE_DRIVER_VERSION_INTEL_NEO1 0x454E4831 // Driver version is ENH1

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

using cl_mem_properties_intel = cl_bitfield;
using cl_mem_flags_intel = cl_mem_flags;
using cl_mem_info_intel = cl_uint;
using cl_mem_advice_intel = cl_uint;
using cl_unified_shared_memory_type_intel = cl_uint;
using cl_unified_shared_memory_capabilities_intel = cl_bitfield;

/******************************
 * Internal only cl_mem_flags *
 ******************************/

#define CL_MEM_FLAGS_INTEL 0x10001
#define CL_MEM_LOCALLY_UNCACHED_RESOURCE (1 << 18)
#define CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE (1 << 25)

// Used with clEnqueueVerifyMemory
#define CL_MEM_COMPARE_EQUAL 0u
#define CL_MEM_COMPARE_NOT_EQUAL 1u

#define CL_MEM_FORCE_LINEAR_STORAGE_INTEL (1 << 19)
#define CL_MEM_FORCE_SHARED_PHYSICAL_MEMORY_INTEL (1 << 20)

#define CL_MEM_ALLOCATION_HANDLE_INTEL 0x10050

//Used with createBuffer
#define CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL (1 << 23)

typedef cl_uint cl_execution_info_intel;
#define CL_EXECUTION_INFO_MAX_WORKGROUP_COUNT_INTEL 0x10100

/******************************
*        UNIFIED MEMORY       *
*******************************/

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

/* cl_mem_alloc_flags_intel - bitfield */
#define CL_MEM_ALLOC_DEFAULT_INTEL 0
#define CL_MEM_ALLOC_WRITE_COMBINED_INTEL (1 << 0)

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

/* cl_kernel_exec_info */
#define CL_KERNEL_EXEC_INFO_INDIRECT_HOST_ACCESS_INTEL 0x4200
#define CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL 0x4201
#define CL_KERNEL_EXEC_INFO_INDIRECT_SHARED_ACCESS_INTEL 0x4202
#define CL_KERNEL_EXEC_INFO_USM_PTRS_INTEL 0x4203

/* cl_command_type */
#define CL_COMMAND_MEMSET_INTEL 0x4204
#define CL_COMMAND_MEMFILL_INTEL 0x4204
#define CL_COMMAND_MEMCPY_INTEL 0x4205
#define CL_COMMAND_MIGRATEMEM_INTEL 0x4206
#define CL_COMMAND_MEMADVISE_INTEL 0x4207

/******************************
*    SLICE COUNT SELECTING    *
*******************************/

/* cl_device_info */
#define CL_DEVICE_SLICE_COUNT_INTEL 0x10020

/* cl_queue_properties */
#define CL_QUEUE_SLICE_COUNT_INTEL 0x10021
