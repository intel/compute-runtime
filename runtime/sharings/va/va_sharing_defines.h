/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "CL/cl.h"
#include "CL/cl_va_api_media_sharing_intel.h"

typedef int (*VADisplayIsValidPFN)(VADisplay vaDisplay);
typedef VAStatus (*VADeriveImagePFN)(VADisplay vaDisplay, VASurfaceID vaSurface, VAImage *vaImage);
typedef VAStatus (*VADestroyImagePFN)(VADisplay vaDisplay, VAImageID vaImageId);
typedef VAStatus (*VAExtGetSurfaceHandlePFN)(VADisplay vaDisplay, VASurfaceID *vaSurface, unsigned int *handleId);
typedef VAStatus (*VASyncSurfacePFN)(VADisplay vaDisplay, VASurfaceID vaSurface);
typedef void *(*VAGetLibFuncPFN)(VADisplay vaDisplay, const char *func);
typedef VAStatus (*VAQueryImageFormatsPFN)(VADisplay vaDisplay, VAImageFormat *formatList, int *numFormats);
typedef int (*VAMaxNumImageFormatsPFN)(VADisplay vaDisplay);
