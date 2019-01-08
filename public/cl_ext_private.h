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

/******************************
 * Internal only cl_mem_flags *
 ******************************/

#define CL_MEM_FLAGS_INTEL 0x10001

// Used with clEnqueueVerifyMemory
#define CL_MEM_COMPARE_EQUAL 0u
#define CL_MEM_COMPARE_NOT_EQUAL 1u

#define CL_MEM_FORCE_SHARED_PHYSICAL_MEMORY_INTEL (1 << 20)
