/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/api/api.h"
#include "runtime/api/dispatch.h"
#include "runtime/context/context.h"
#include "runtime/helpers/get_info.h"
#include "runtime/mem_obj/image.h"
#include "runtime/mem_obj/mem_obj.h"

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
