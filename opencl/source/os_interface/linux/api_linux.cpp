/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/get_info.h"

#include "opencl/source/api/api.h"
#include "opencl/source/api/dispatch.h"
#include "opencl/source/context/context.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/mem_obj/mem_obj.h"

void NEO::MemObj::getOsSpecificMemObjectInfo(const cl_mem_info &paramName, size_t *srcParamSize, void **srcParam) {
    switch (paramName) {
#ifdef LIBVA
    case CL_MEM_VA_API_MEDIA_SURFACE_INTEL:
        peekSharingHandler()->getMemObjectInfo(*srcParamSize, *srcParam);
        break;
#endif
    default:
        break;
    }
}

void NEO::Image::getOsSpecificImageInfo(const cl_image_info &paramName, size_t *srcParamSize, void **srcParam) {
    switch (paramName) {
#ifdef LIBVA
    case CL_IMAGE_VA_API_PLANE_INTEL:
        *srcParamSize = sizeof(cl_uint);
        *srcParam = &mediaPlaneType;
        break;
#endif
    default:
        break;
    }
}

void *NEO::Context::getOsContextInfo(cl_context_info &paramName, size_t *srcParamSize) {
    return nullptr;
}
