/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/tracing/tracing_types.h"

#ifdef __cplusplus
extern "C" {
#endif

cl_int CL_API_CALL clCreateTracingHandleINTEL(cl_device_id device, cl_tracing_callback callback, void *userData, cl_tracing_handle *handle);
cl_int CL_API_CALL clSetTracingPointINTEL(cl_tracing_handle handle, cl_function_id fid, cl_bool enable);
cl_int CL_API_CALL clDestroyTracingHandleINTEL(cl_tracing_handle handle);

cl_int CL_API_CALL clEnableTracingINTEL(cl_tracing_handle handle);
cl_int CL_API_CALL clDisableTracingINTEL(cl_tracing_handle handle);
cl_int CL_API_CALL clGetTracingStateINTEL(cl_tracing_handle handle, cl_bool *enable);

#ifdef __cplusplus
}
#endif
