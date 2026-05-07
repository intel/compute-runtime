/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "CL/cl.h"

#ifndef CL_API_SUFFIX__VERSION_3_1
#define CL_API_SUFFIX__VERSION_3_1 CL_API_SUFFIX_COMMON
#endif

#ifndef CL_VERSION_3_1

#if !defined(CL_NO_CORE_PROTOTYPES)

#ifdef __cplusplus
extern "C" {
#endif

extern CL_API_ENTRY cl_int CL_API_CALL
clGetKernelSuggestedLocalWorkSize(
    cl_command_queue command_queue,
    cl_kernel kernel,
    cl_uint work_dim,
    const size_t *global_work_offset,
    const size_t *global_work_size,
    size_t *suggested_local_work_size) CL_API_SUFFIX__VERSION_3_1;

#ifdef __cplusplus
}
#endif

#endif

#endif
