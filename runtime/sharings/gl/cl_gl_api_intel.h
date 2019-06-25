/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "CL/cl_gl.h"

extern cl_int CL_API_CALL clGetSupportedGLTextureFormatsINTEL(
    cl_context context,
    cl_mem_flags flags,
    cl_mem_object_type imageType,
    cl_uint numEntries,
    cl_GLenum *glFormats,
    cl_uint *numTextureFormats);
