/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "CL/cl.h"
#include "CL/cl_va_api_media_sharing_intel.h"

cl_int CL_API_CALL clGetSupportedVA_APIMediaSurfaceFormatsINTEL(
    cl_context context,
    cl_mem_flags flags,
    cl_mem_object_type image_type,
    cl_uint num_entries,
    VAImageFormat *va_api_formats,
    cl_uint *num_image_formats);
