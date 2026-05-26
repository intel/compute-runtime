/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/context/context.h"
#include "level_zero/api/opencl/source/mem_obj/image.h"
#include "level_zero/api/opencl/source/sharings/sharing.h"
#include "level_zero/core/source/image/image_imp.h"

void NEO::LEO::MemObj::getOsSpecificMemObjectInfo(const cl_mem_info &paramName, size_t *srcParamSize, void **srcParam) {
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

void NEO::LEO::Image::getOsSpecificImageInfo(const cl_image_info &paramName, size_t *srcParamSize, void **srcParam) {
    this->mediaPlane = static_cast<cl_uint>(static_cast<L0::ImageImp *>(L0::Image::fromHandle(this->imageHandle))->getImageInfo().plane) - 1;
    switch (paramName) {
#ifdef LIBVA
    case CL_IMAGE_VA_API_PLANE_INTEL:
        *srcParamSize = sizeof(cl_uint);
        *srcParam = &this->mediaPlane;
        break;
#endif
    default:
        break;
    }
}

void *NEO::LEO::Context::getOsContextInfo(cl_context_info &paramName, size_t *srcParamSize) {
    return nullptr;
}
