/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "CL/cl.h"
#include "CL/cl_va_api_media_sharing_intel.h"

cl_int CL_API_CALL clGetSupportedVA_APIMediaSurfaceFormatsINTEL( // NOLINT(readability-identifier-naming)
    cl_context context,
    cl_mem_flags flags,
    cl_mem_object_type imageType,
    cl_uint plane,
    cl_uint numEntries,
    VAImageFormat *vaApiFormats,
    cl_uint *numImageFormats);
